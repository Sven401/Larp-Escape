#include "ProximityBleCrypto.h"
#include <psa/crypto.h>
namespace larp { namespace proximity { namespace ble {
namespace {
struct Key {
    mbedtls_svc_key_id_t value=0;
    Key()=default;Key(const Key&)=delete;Key&operator=(const Key&)=delete;
    bool close(){if(!value)return true;if(psa_destroy_key(value)!=PSA_SUCCESS)return false;value=0;return true;}
    ~Key(){close();}
};
}
bool NativeCrypto::hmac(const Secret &secret,const std::uint8_t *data,std::size_t count,Secret &out){
    if(psa_crypto_init()!=PSA_SUCCESS)return false;
    Key key;psa_key_attributes_t attr=PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr,PSA_KEY_TYPE_HMAC);psa_set_key_bits(&attr,256);
    psa_set_key_usage_flags(&attr,PSA_KEY_USAGE_SIGN_MESSAGE);psa_set_key_algorithm(&attr,PSA_ALG_HMAC(PSA_ALG_SHA_256));
    const auto imported=psa_import_key(&attr,secret.data(),secret.size(),&key.value);psa_reset_key_attributes(&attr);
    if(imported!=PSA_SUCCESS)return false;
    std::size_t size=0;
    const bool computed=psa_mac_compute(key.value,PSA_ALG_HMAC(PSA_ALG_SHA_256),data,count,out.data(),out.size(),&size)==PSA_SUCCESS&&size==32;
    const bool closed=key.close();return computed&&closed;
}
bool NativeCrypto::hash(const std::uint8_t *data,std::size_t count,Secret &out){
    if(psa_crypto_init()!=PSA_SUCCESS)return false;
    std::size_t size=0;return psa_hash_compute(PSA_ALG_SHA_256,data,count,out.data(),out.size(),&size)==PSA_SUCCESS&&size==32;
}
}}}
