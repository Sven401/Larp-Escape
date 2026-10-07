"""Independent correlation of actual C++ actor observation with persisted public records.

SDK callbacks, NVS, task identities and transport security remain deterministic
Host models. This counterpart never converts those into device/RF measurements.
"""
from pathlib import Path
import copy
import json
from check_peer_registry import decode

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / 'docs/companion/fixtures/peer-registry'


def expected_observation():
    return dict(
        scope='host_actual_crypto_original_sdk_actor_nvs_model', recordBytes=404,
        roleSlots=1, rolesMaximum=4, roleSdkPosts=10, attSdkPosts=13, sdkPosts=23,
        setCalls=3, commitCalls=3, proofFrames=6, commandFrames=13,
        registryWorker=2, sourceActor=1, cryptoWorker=3, responseWorker=4,
        applicationWorker=5, jobDeadlineMilliseconds=5000, operationMilliseconds=30000,
        restoredExplicitRole=True, closeAckBeforeNvs=True,
        installAckBeforePublication=True, cryptoReadsProtectedCopy=True,
        proofCurrentAfterVerification=True, runningAfterRevocation=True,
        oldProofRejected=True, transportSecurityModeled=True,
        physicalDurabilityMeasured=False, smpMeasured=False, rfInitialized=False,
    )


def accept(value, records):
    expected = expected_observation()
    if not isinstance(value, dict) or value != expected:
        raise ValueError('actor scope/value')
    if any(type(value[key]) is not type(wanted) for key, wanted in expected.items()):
        raise ValueError('actor field type')
    # Two original closed SDK ACKs per startup, key edit, explicit-role edit,
    # fresh-owner restore, and revoke. Only edits write the NVS model.
    if value['roleSdkPosts'] != 2 * (2 + len(records)):
        raise ValueError('role handoff correlation')
    if value['sdkPosts'] != value['roleSdkPosts'] + value['attSdkPosts']:
        raise ValueError('SDK post correlation')
    if value['attSdkPosts'] != value['commandFrames']:
        raise ValueError('original ATT response correlation')
    if value['setCalls'] != len(records) or value['commitCalls'] != len(records):
        raise ValueError('persisted-edit correlation')
    if [record['generation'] for record in records] != [1, 2, 3]:
        raise ValueError('registry generations')
    if [record['entries'][0]['principal'] for record in records] != [0, 7, 0]:
        raise ValueError('explicit role restore/revoke')
    if any(len(record['entries']) != 1 for record in records):
        raise ValueError('peer count')
    first = records[0]
    if any(record['device'] != first['device'] or
           record['entries'][0]['peer'] != first['entries'][0]['peer'] or
           record['entries'][0]['key'] != first['entries'][0]['key'] or
           record['entries'][0]['credential'] != 1 for record in records):
        raise ValueError('frozen credential correlation')


def main():
    records = [decode(bytes.fromhex((FOLDER / (name + '.hex')).read_text(encoding='utf-8').strip()))
               for name in ('key-only', 'game-master', 'revoked')]
    value = json.loads((FOLDER / 'actor-observation.json').read_text(encoding='utf-8'))
    accept(value, records)
    negatives = []
    for key, wanted in value.items():
        wrong = dict(value)
        wrong[key] = (not wanted if isinstance(wanted, bool) else
                      wanted + 1 if isinstance(wanted, int) else 'device_measured')
        negatives.append(wrong)
        wrong = dict(value)
        wrong[key] = (int(wanted) if isinstance(wanted, bool) else
                      str(wanted) if isinstance(wanted, int) else None)
        negatives.append(wrong)
    negatives.extend([dict(value, unexpected=True), {k: v for k, v in value.items() if k != 'scope'}])
    for wrong in negatives:
        try:
            accept(wrong, records)
        except ValueError:
            continue
        raise ValueError('invalid actor observation accepted')
    record_negatives = []
    for variant in range(6):
        wrong = copy.deepcopy(records)
        if variant == 0:
            wrong[1]['entries'][0]['principal'] = 0
        elif variant == 1:
            wrong[2]['entries'][0]['principal'] = 7
        elif variant == 2:
            wrong[2]['generation'] = 2
        elif variant == 3:
            wrong[2]['entries'][0]['credential'] = 2
        elif variant == 4:
            wrong[1]['device'] = bytes(16)
        else:
            wrong[2]['entries'][0]['key'] = bytes(65)
        record_negatives.append(wrong)
    for wrong in record_negatives:
        try:
            accept(value, wrong)
        except ValueError:
            continue
        raise ValueError('invalid actor/record correlation accepted')
    print(f'Peer registry actor counterpart: 3 actual C++ records; 10 role SDK posts + 13 original '
          f'ATT posts = 23; 5 model workers; {len(negatives)} wrong observations and '
          f'{len(record_negatives)} wrong record correlations rejected; no device/RF/SMP claim')


if __name__ == '__main__':
    main()
