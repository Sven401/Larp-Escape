import {test} from 'node:test';
import assert from 'node:assert/strict';
import {initial,reducer,canCommand} from '../src/pilot.ts';
const act=(s,...actions)=>actions.reduce(reducer,s);
const ready=()=>act({...initial},{type:'OPEN'},{type:'FOUND',empty:false,matches:1},{type:'VERIFIED',denied:false});
test('commands require fresh authenticated station state',()=>{for(const phase of ['idle','verifying','lost','stale','access']){const s={...initial,screen:'station',phase};assert.equal(canCommand(s),false);assert.equal(reducer(s,{type:'START'}).commandCount,0);}});
test('selection is local and never sends a command',()=>{const s=reducer(ready(),{type:'SELECT',effect:'Ripple'});assert.equal(s.commandCount,0);assert.equal(s.world,null);});
test('one in-flight command blocks another',()=>{const s=act(ready(),{type:'START'},{type:'START'});assert.equal(s.commandCount,1);assert.equal(s.active,null);});
test('loss before response invalidates display and marks unknown outcome',()=>{const s=act(ready(),{type:'START'},{type:'LOST'});assert.equal(s.phase,'lost');assert.equal(s.active,null);assert.equal(s.uncertain,true);assert.equal(canCommand(s),false);assert.equal(reducer(s,{type:'ACK'}),s);});
test('reconnect reads simulated device state without replay',()=>{const s=act(ready(),{type:'SELECT',effect:'Ripple'},{type:'START'},{type:'LOST',delivered:true},{type:'SCAN'},{type:'FOUND',empty:false,matches:1},{type:'VERIFIED',denied:false});assert.equal(s.commandCount,1);assert.equal(s.active,'Ripple');assert.equal(s.uncertain,false);assert.equal(s.selected,'Ripple');});
test('stale state disables controls without inventing stop',()=>{const s=act(ready(),{type:'START'},{type:'ACK'},{type:'STALE'});assert.equal(s.active,null);assert.equal(s.world,'Funkeln');assert.equal(canCommand(s),false);});
test('late scan and auth callbacks after cancel have no effect',()=>{for(const a of [{type:'FOUND',empty:false,matches:1},{type:'VERIFIED',denied:false}]){const s=reducer(ready(),{type:'CANCEL'});assert.equal(reducer(s,a),s);}});
test('denied access does not enable a command',()=>{const s=act(ready(),{type:'SCAN'},{type:'FOUND',empty:false,matches:1},{type:'VERIFIED',denied:true});assert.equal(s.phase,'access');assert.equal(canCommand(s),false);assert.equal(s.active,null);});
test('background preserves uncertainty and closes session',()=>{const s=act(ready(),{type:'START'},{type:'BACKGROUND'});assert.equal(s.phase,'background');assert.equal(s.uncertain,true);assert.equal(s.active,null);});

test('configured station opens directly into automatic connection',()=>{let s=reducer({...initial},{type:'OPEN'});assert.equal(s.phase,'scanning');s=reducer(s,{type:'FOUND',empty:false,matches:1});assert.equal(s.phase,'verifying');s=reducer(s,{type:'VERIFIED',denied:false});assert.equal(s.phase,'ready');assert.equal(s.commandCount,0);});
test('ambiguous station mapping never chooses an arbitrary candidate',()=>{const s=act({...initial},{type:'OPEN'},{type:'FOUND',empty:false,matches:2});assert.equal(s.phase,'assignment');assert.equal(canCommand(s),false);assert.equal(reducer(s,{type:'VERIFIED',denied:false}),s);});
test('missing OS permission remains an explicit exception',()=>{const s=reducer({...initial},{type:'OPEN',blocked:'permission'});assert.equal(s.phase,'permission');assert.equal(s.permission,false);assert.equal(reducer(s,{type:'DENY'}).phase,'denied');assert.equal(reducer(s,{type:'DENY'}).permission,false);});
test('return to station reconnects but does not replay a previous action',()=>{const s=act(ready(),{type:'START'},{type:'BACK'},{type:'OPEN'},{type:'FOUND',empty:false,matches:1},{type:'VERIFIED',denied:false});assert.equal(s.phase,'ready');assert.equal(s.commandCount,1);});

test('missing configured station match never authenticates',()=>{const s=act({...initial},{type:"OPEN"},{type:"FOUND",empty:false,matches:0});assert.equal(s.phase,"assignment");assert.equal(canCommand(s),false);});

for (const delivered of [true,false]) {
  for (const command of ['START','STOP']) {
    test(`${command}: lost response, delivered=${delivered}, fresh read without replay`,()=>{
      const before=command==='STOP'?act(ready(),{type:'START'},{type:'ACK'}):ready();
      const pending=reducer(before,{type:command});
      assert.equal(pending.world,before.world);
      const lost=reducer(pending,{type:'LOST',delivered});
      assert.equal(lost.active,null);
      assert.equal(lost.uncertain,true);
      assert.equal(canCommand(lost),false);
      const expected=delivered?(command==='STOP'?null:'Funkeln'):before.world;
      const resumed=act(lost,{type:'SCAN'},{type:'FOUND',empty:false,matches:1},{type:'VERIFIED',denied:false});
      assert.equal(resumed.active,expected);
      assert.equal(resumed.commandCount,pending.commandCount);
      assert.equal(resumed.uncertain,false);
      assert.equal(resumed.selected,'Funkeln');
      assert.equal(reducer(resumed,{type:'ACK'}),resumed);
    });
  }
}
test('retry never repairs an ambiguous mapping or invalid access',()=>{
  let mapping=act(ready(),{type:'SCAN'},{type:'FOUND',empty:false,matches:2});
  mapping=act(mapping,{type:'SCAN'},{type:'FOUND',empty:false,matches:2});
  assert.equal(mapping.phase,'assignment');
  assert.equal(canCommand(mapping),false);
  let denied=act(ready(),{type:'SCAN'},{type:'FOUND',empty:false,matches:1},{type:'VERIFIED',denied:true});
  denied=act(denied,{type:'SCAN'},{type:'FOUND',empty:false,matches:1},{type:'VERIFIED',denied:true});
  assert.equal(denied.phase,'access');
  assert.equal(canCommand(denied),false);
  const repaired=act(denied,{type:'SCAN'},{type:'FOUND',empty:false,matches:1},{type:'VERIFIED',denied:false});
  assert.equal(repaired.phase,'ready');
  assert.equal(repaired.commandCount,0);
});
