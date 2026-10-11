
var assert = require("assert"), cases = 0;
function run(f) { f(); cases++; }
function setup(source, delay) {
  var sends=[], c=WirelessFlashController(function(v){sends.push(v);});
  assert(c.configure(true,source,delay));
  return {c:c,s:sends};
}
for (var src=0;src<4;src++) (function(src) {
 run(function(){
  var x=setup(src,100); x.c.begin(1,true,0);
  for(var p=1;p<4;p++) x.c.phase(1,p,p*10);
  assert(!x.c.tick(src*10+99)); assert(x.c.tick(src*10+100));
  assert.equal(x.s.length,1); assert.equal(x.s[0].source,src);
  for(var k=0;k<4;k++) x.c.phase(1,k,200+k);
  x.c.tick(300); assert.equal(x.s.length,1);
 });
 run(function(){
  var x=setup(src,100); x.c.begin(1,true,0);
  for(var p=1;p<=src;p++) x.c.phase(1,p,p*10);
  x.c.end(1); x.c.tick(1000); assert.equal(x.s.length,0);
 });
 run(function(){
  var x=setup(src,100); x.c.begin(1,true,0);
  for(var p=1;p<=src;p++) x.c.phase(1,p,p*10);
  x.c.configure(false,src,100); x.c.tick(1000); assert.equal(x.s.length,0);
 });
})(src);
run(function(){var x=setup(0,100);x.c.begin(1,false,0);x.c.tick(100);assert.equal(x.s.length,0);});
run(function(){var x=setup(0,100);x.c.begin(1,true,0);x.c.begin(2,true,50);x.c.tick(100);assert.equal(x.s.length,0);x.c.tick(150);assert.equal(x.s.length,1);assert.equal(x.s[0].shot,2);});
run(function(){var x=setup(2,0);x.c.begin(2,true,0);x.c.phase(1,2,10);x.c.tick(10);assert.equal(x.s.length,0);x.c.phase(2,2,20);x.c.tick(20);assert.equal(x.s.length,1);});
run(function(){var x=setup(0,100);x.c.begin(1,true,0);x.c.tick(400);assert.equal(x.s.length,0);assert.equal(x.c.inspect().error,"deadline_missed");});
run(function(){var x=setup(0,100);x.c.begin(1,true,100);x.c.tick(99);assert.equal(x.s.length,0);assert.equal(x.c.inspect().error,"invalid_clock");});
run(function(){var x=setup(1,0);x.c.begin(1,true,0);x.c.phase(1,3,10);x.c.phase(1,1,20);x.c.tick(20);assert.equal(x.s.length,0);});
run(function(){var x=setup(0,0);x.c.begin(1,true,0);x.c.tick(0);x.c.end(1);x.c.begin(1,true,1);x.c.tick(1);assert.equal(x.s.length,1);});
run(function(){var x=setup(0,0);x.c.begin(1,true,0);x.c.cancel("sleep");x.c.tick(0);assert.equal(x.s.length,0);});
[-1,5001,NaN,Infinity,0.5,"100",null].forEach(function(v){run(function(){var x=setup(0,100);x.c.begin(1,true,0);assert(!x.c.configure(true,0,v));x.c.tick(100);assert.equal(x.s.length,0);});});
[-1,4,0.5,"1",null].forEach(function(v){run(function(){var x=setup(0,100);x.c.begin(1,true,0);assert(!x.c.configure(true,v,100));x.c.tick(100);assert.equal(x.s.length,0);});});
run(function(){var n=0,c=WirelessFlashController(function(){n++;throw Error("stub");});c.configure(true,0,0);c.begin(1,true,0);assert(!c.tick(0));assert(!c.tick(1));assert.equal(n,1);});
process.stdout.write(JSON.stringify({passed:cases,deviceRequests:0,filesystemWrites:0}));

