from pathlib import Path
import sys, ssl, subprocess, json, os, datetime, tempfile, threading, queue, time

from cryptography import x509
from cryptography.hazmat.primitives import hashes,serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.x509.oid import NameOID,ExtendedKeyUsageOID
executable=Path(sys.argv[1]).resolve()
temporary=tempfile.TemporaryDirectory(prefix='larp-native-tls-')
base=Path(temporary.name)
for name,scalar,purpose in [('server',2,ExtendedKeyUsageOID.SERVER_AUTH),('client',1,ExtendedKeyUsageOID.CLIENT_AUTH),('wrong',3,ExtendedKeyUsageOID.CLIENT_AUTH)]:
 key=ec.derive_private_key(scalar,ec.SECP256R1())
 subject=x509.Name([x509.NameAttribute(NameOID.COMMON_NAME,'TEST ONLY '+name)])
 cert=(x509.CertificateBuilder().subject_name(subject).issuer_name(subject).public_key(key.public_key()).serial_number(scalar)
  .not_valid_before(datetime.datetime(1970,1,1,tzinfo=datetime.timezone.utc)).not_valid_after(datetime.datetime(2100,1,1,tzinfo=datetime.timezone.utc))
  .add_extension(x509.BasicConstraints(ca=False,path_length=None),True)
  .add_extension(x509.KeyUsage(True,False,False,False,True,False,False,False,False),True)
  .add_extension(x509.ExtendedKeyUsage([purpose]),False).sign(key,hashes.SHA256()))
 (base/(name+'.pem')).write_bytes(cert.public_bytes(serialization.Encoding.PEM))
 (base/(name+'-key.pem')).write_bytes(key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.TraditionalOpenSSL,serialization.NoEncryption()))

def run(case,fragment=20):
 ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT);ctx.minimum_version=ctx.maximum_version=ssl.TLSVersion.TLSv1_2
 ctx.set_ciphers('ECDHE-ECDSA-AES128-GCM-SHA256');ctx.check_hostname=False
 ctx.load_verify_locations(str(base/('wrong.pem' if case=='wrong_server' else 'server.pem')))
 if case!='missing_client':
  who='wrong' if case=='wrong_client' else 'client';ctx.load_cert_chain(str(base/(who+'.pem')),str(base/(who+'-key.pem')))
 incoming,outgoing=ssl.MemoryBIO(),ssl.MemoryBIO();client=ctx.wrap_bio(incoming,outgoing,server_side=False)
 env=os.environ.copy();
 process=subprocess.Popen([str(executable),str(base/'server.pem'),str(base/'server-key.pem'),str(base/'client.pem')],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=env)
 responses=queue.Queue(maxsize=2)
 def reader():
  for line in process.stdout:
   try: responses.put(line,timeout=1)
   except queue.Full: return
  try: responses.put(None,timeout=1)
  except queue.Full: pass
 threading.Thread(target=reader,daemon=True).start()
 deadline=time.monotonic()+15
 pending=bytearray();received=bytearray();server_plain=bytearray();client_ready=False;rounds=0
 def step(command=None):
  nonlocal rounds
  rounds+=1
  if rounds>6000:raise AssertionError('step budget exceeded')
  pending.extend(outgoing.read())
  chunk=bytes(pending[:fragment]);del pending[:fragment]
  process.stdin.write((command if command is not None else chunk.hex())+'\n');process.stdin.flush()
  remaining=deadline-time.monotonic()
  if remaining<=0: raise AssertionError('native TLS wallclock budget exceeded')
  try: line=responses.get(timeout=min(2,remaining))
  except queue.Empty: raise AssertionError('native TLS response deadline exceeded')
  if not line:raise AssertionError('native exited before response')
  state=json.loads(line);server_plain.extend(bytes.fromhex(state['plain']))
  raw=bytes.fromhex(state['out'])
  assert len(raw)<=20
  if raw:incoming.write(raw)
  return state
 try:
  if case=='input_pressure':
   queued=0
   for _ in range(33):
    state=step('@INPUT_PRESSURE')
    queued+=int(state['accepted'])
    assert not state['error'] and not state['out'] and not server_plain
   assert queued==32
   state=step('@REVOKE')
   assert state['error'] and not state['out'] and not server_plain
   return dict(case=case,queued=queued,overflowRejected=True)
  if case=='handshake_timeout':
   state=step('@EXPIRE')
   assert state['error'] and not state['ready'] and not server_plain and not state['out']
   return dict(case=case,rejected=True,plaintext=0)
  for _ in range(6000):
   if not client_ready:
    try:client.do_handshake();client_ready=True
    except ssl.SSLWantReadError:pass
    except ssl.SSLError as error:
     assert case in ('wrong_client','wrong_server','missing_client'),str(error)
     assert not server_plain
     return dict(case=case,rejected=True,side='client',plaintext=len(server_plain),steps=rounds)
   state=step()
   if state['error']:
    assert case in ('wrong_client','wrong_server','missing_client'),state
    assert not server_plain
    return dict(case=case,rejected=True,side='server',code=state['error'],plaintext=len(server_plain),steps=rounds)
   if client_ready and state['ready'] and not pending and not outgoing.pending:break
  else:raise AssertionError('handshake not complete')
  assert case not in ('wrong_client','wrong_server','missing_client'),'unexpected authentication'
  assert client.version()=='TLSv1.2' and client.cipher()[0]=='ECDHE-ECDSA-AES128-GCM-SHA256'
  if case=='origin_revoke':
   state=step('@REVOKE')
   assert state['error'] and not state['ready'] and not state['out'] and not server_plain
   return dict(case=case,rejected=True,plaintext=0)
  if case in ('stale_origin','stale_begin'):
   state=step('@STALE' if case=='stale_origin' else '@STALE_BEGIN')
   assert not state['accepted'] and not state['error'] and state['ready'] and not state['out']
  if case in ('output_pressure','revocation_pending'):
   queued=0
   for _ in range(60):
    state=step('@PRESSURE')
    assert not state['error'] and not state['out'] and not state['plain']
    queued+=int(state['accepted'])
   assert 0<queued<60
   if case=='revocation_pending':
    state=step('@REVOKE')
    assert state['error'] and not state['ready'] and not state['out'] and not server_plain
    return dict(case=case,rejected=True,queuedCiphertextDiscarded=True)
   for _ in range(6000):
    state=step()
    assert not state['error']
    try: received.extend(client.read(16384))
    except ssl.SSLWantReadError: pass
    if len(received)==queued*128:break
   else: raise AssertionError('backpressured output did not drain')
   assert received==bytes([7])*(queued*128)
   return dict(case=case,queued=queued,bytes=len(received),callerCopyPreserved=True)
  message=b'authorized test payload, no role granted'
  assert client.write(message)==len(message)
  cipher=outgoing.read()
  if case=='tamper':cipher=cipher[:-1]+bytes([cipher[-1]^1])
  if case=='plaintext':cipher=b'not-a-tls-record-or-authorized-command'
  if case=='partial_abort':
   pending.extend(cipher[:len(cipher)//2])
   while pending: step()
   assert not server_plain
   state=step('@CLOSE')
   assert not state['ready'] and not state['out'] and not server_plain
   return dict(case=case,closed=True,plaintext=0)
  pending.extend(cipher)
  for _ in range(6000):
   state=step()
   if state['error']:
    assert case in ('tamper','plaintext'),state
    assert not server_plain and not received
    return dict(case=case,rejected=True,code=state['error'],plaintext=0,steps=rounds)
   try:received.extend(client.read(1024))
   except ssl.SSLWantReadError:pass
   if received==message and not pending:break
  else:raise AssertionError('echo not complete')
  assert case not in ('tamper','plaintext'),'invalid record accepted'
  assert server_plain==message and received==message
  if case=='reconnect_replay':
   before=len(server_plain)
   state=step('@REOPEN')
   assert state['accepted'] and not state['ready']
   pending.extend(cipher)
   for _ in range(6000):
    state=step()
    if state['error']:
     assert len(server_plain)==before
     return dict(case=case,rejected=True,extraPlaintext=0)
   raise AssertionError('ciphertext from previous TLS connection not rejected')
  if case=='replay':
   before=len(server_plain);pending.extend(cipher)
   for _ in range(6000):
    state=step()
    if state['error']:
     assert len(server_plain)==before
     return dict(case=case,rejected=True,code=state['error'],extraPlaintext=0,steps=rounds)
   raise AssertionError('replay not rejected')
  return dict(case=case,fragment=fragment,cipher=client.cipher()[0],version=client.version(),echoBytes=len(received),steps=rounds)
 finally:
  process.stdin.close()
  try: process.wait(timeout=1)
  except subprocess.TimeoutExpired:
   process.terminate()
   try: process.wait(timeout=1)
   except subprocess.TimeoutExpired:
    process.kill()
    process.wait(timeout=1)
results=[run('normal',n) for n in (1,7,20)]+[run(x) for x in ('wrong_client','wrong_server','missing_client','tamper','plaintext','replay','handshake_timeout','origin_revoke','stale_origin','stale_begin','output_pressure','input_pressure','revocation_pending','partial_abort','reconnect_replay')]
(base/'results.json').write_text(json.dumps({'scope':'Native pinned backend TLS host regression only - no device or role grant','deviceMeasured':False,'roleGranted':False,'cases':results},indent=2),encoding='utf-8')
print(json.dumps(results,indent=2))
