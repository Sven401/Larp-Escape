#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
// HOST FEASIBILITY ONLY: no device, roles or product trust is granted here.
struct Ring {
    std::array<unsigned char, 4096> bytes{};
    size_t head=0, count=0;
    int put(const unsigned char* p,size_t n) { n=std::min(n,bytes.size()-count); for(size_t i=0;i<n;i++) bytes[(head+count+i)%bytes.size()]=p[i]; count+=n; return int(n); }
    int take(unsigned char* p,size_t n) { n=std::min(n,count);for(size_t i=0;i<n;i++)p[i]=bytes[(head+i)%bytes.size()];head=(head+n)%bytes.size();count-=n;return int(n); }
};
struct Bio { Ring in,out; };
int sendBytes(void* p,const unsigned char* b,size_t n) { auto& out=static_cast<Bio*>(p)->out; if(out.count==out.bytes.size()) return MBEDTLS_ERR_SSL_WANT_WRITE; return out.put(b,std::min(n,size_t(20))); }
int receiveBytes(void* p,unsigned char* b,size_t n) { auto& in=static_cast<Bio*>(p)->in; if(!in.count)return MBEDTLS_ERR_SSL_WANT_READ;return in.take(b,std::min(n,size_t(20))); }
std::string file(const char* path) { std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("missing fixture");return {std::istreambuf_iterator<char>(f),{}}; }
std::string hex(const unsigned char* b,size_t n) {std::string s;const char* h="0123456789abcdef";for(size_t i=0;i<n;i++){s+=h[b[i]>>4];s+=h[b[i]&15];}return s;}
struct Pin { std::array<unsigned char,256> der{};int count=0; };
int verifyPin(void* p,mbedtls_x509_crt* c,int depth,uint32_t* flags) {
    auto& pin=*static_cast<Pin*>(p); std::array<unsigned char,256> der{};
    int n=mbedtls_pk_write_pubkey_der(&c->pk,der.data(),der.size());
    if(depth!=0 || n!=pin.count || n<=0 || std::memcmp(der.data()+der.size()-n,pin.der.data()+pin.der.size()-n,n)) return MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
    // Only the explicit self-signed trust-anchor exception. Keep all other flags.
    *flags &= ~MBEDTLS_X509_BADCERT_NOT_TRUSTED;
    return *flags ? MBEDTLS_ERR_X509_CERT_VERIFY_FAILED : 0;
}
int main(int argc,char** argv) {
    if(argc!=4)return 2;
    mbedtls_ssl_context ssl;mbedtls_ssl_config config;mbedtls_x509_crt cert,peer;
    mbedtls_pk_context key;mbedtls_entropy_context entropy;mbedtls_ctr_drbg_context random;
    mbedtls_ssl_init(&ssl);mbedtls_ssl_config_init(&config);mbedtls_x509_crt_init(&cert);mbedtls_x509_crt_init(&peer);
    mbedtls_pk_init(&key);mbedtls_entropy_init(&entropy);mbedtls_ctr_drbg_init(&random);
    int error=0;bool ready=false;Bio bio;Pin pin;
    auto c=file(argv[1]),k=file(argv[2]),p=file(argv[3]);
    const unsigned char purpose[]="Larp TLS host feasibility - TEST ONLY";
    if((error=mbedtls_ctr_drbg_seed(&random,mbedtls_entropy_func,&entropy,purpose,sizeof(purpose)-1)) ||
       (error=mbedtls_x509_crt_parse(&cert,reinterpret_cast<const unsigned char*>(c.c_str()),c.size()+1)) ||
       (error=mbedtls_x509_crt_parse(&peer,reinterpret_cast<const unsigned char*>(p.c_str()),p.size()+1)) ||
       (error=mbedtls_pk_parse_key(&key,reinterpret_cast<const unsigned char*>(k.c_str()),k.size()+1,nullptr,0)) ||
       (error=mbedtls_ssl_config_defaults(&config,MBEDTLS_SSL_IS_SERVER,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT))) {
        std::cerr<<"init "<<error<<"\n";return 3;
    }
    pin.count=mbedtls_pk_write_pubkey_der(&peer.pk,pin.der.data(),pin.der.size());
    static const int suites[]={MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,0};
    static const mbedtls_ecp_group_id curves[]={MBEDTLS_ECP_DP_SECP256R1,MBEDTLS_ECP_DP_NONE};
    static const int hashes[]={MBEDTLS_MD_SHA256,MBEDTLS_MD_NONE};
    mbedtls_ssl_conf_rng(&config,mbedtls_ctr_drbg_random,&random);
    mbedtls_ssl_conf_authmode(&config,MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_verify(&config,verifyPin,&pin);
    mbedtls_ssl_conf_ca_chain(&config,&peer,nullptr);
    mbedtls_ssl_conf_ciphersuites(&config,suites);mbedtls_ssl_conf_curves(&config,curves);mbedtls_ssl_conf_sig_hashes(&config,hashes);
    mbedtls_ssl_conf_min_version(&config,MBEDTLS_SSL_MAJOR_VERSION_3,MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_max_version(&config,MBEDTLS_SSL_MAJOR_VERSION_3,MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_renegotiation(&config,MBEDTLS_SSL_RENEGOTIATION_DISABLED);
    mbedtls_ssl_conf_session_tickets(&config,MBEDTLS_SSL_SESSION_TICKETS_DISABLED);
    mbedtls_ssl_conf_session_cache(&config,nullptr,nullptr,nullptr);
    if((error=mbedtls_ssl_conf_own_cert(&config,&cert,&key)) || (error=mbedtls_ssl_setup(&ssl,&config))) return 4;
    mbedtls_ssl_set_bio(&ssl,&bio,sendBytes,receiveBytes,nullptr);
    std::array<unsigned char,128> plain{}, echo{};size_t echoSize=0,echoOffset=0;
    std::string line;
    while(std::getline(std::cin,line)) {
        if(line.size()>40 || line.size()%2) return 5;
        std::array<unsigned char,20> input{};
        for(size_t i=0;i<line.size()/2;i++)input[i]=static_cast<unsigned char>(std::stoul(line.substr(i*2,2),nullptr,16));
        if(bio.in.put(input.data(),line.size()/2)!=int(line.size()/2))return 6;
        size_t got=0;
        if(!error && !ready) {int result=mbedtls_ssl_handshake(&ssl);if(result==0){ready=mbedtls_ssl_get_verify_result(&ssl)==0;if(!ready)error=-1;}else if(result!=MBEDTLS_ERR_SSL_WANT_READ && result!=MBEDTLS_ERR_SSL_WANT_WRITE)error=result;}
        if(!error && ready) {
            if(echoSize) {
                int n=mbedtls_ssl_write(&ssl,echo.data()+echoOffset,echoSize-echoOffset);
                if(n>0){echoOffset+=n;if(echoOffset==echoSize)echoSize=echoOffset=0;}
                else if(n!=MBEDTLS_ERR_SSL_WANT_READ && n!=MBEDTLS_ERR_SSL_WANT_WRITE)error=n?n:-2;
            } else {
                int n=mbedtls_ssl_read(&ssl,plain.data(),plain.size());
                if(n>0){got=n;std::copy_n(plain.data(),got,echo.data());echoSize=got;}
                else if(n!=MBEDTLS_ERR_SSL_WANT_READ && n!=MBEDTLS_ERR_SSL_WANT_WRITE)error=n?n:-3;
            }
        }
        std::array<unsigned char,20> out{};int n=bio.out.take(out.data(),out.size());
        std::cout<<"{\"out\":\""<<hex(out.data(),n)<<"\",\"ready\":"<<(ready?"true":"false")<<",\"error\":"<<error<<",\"plain\":\""<<hex(plain.data(),got)<<"\"}"<<std::endl;
    }
    mbedtls_ssl_free(&ssl);mbedtls_ssl_config_free(&config);mbedtls_x509_crt_free(&cert);mbedtls_x509_crt_free(&peer);mbedtls_pk_free(&key);mbedtls_ctr_drbg_free(&random);mbedtls_entropy_free(&entropy);
}
