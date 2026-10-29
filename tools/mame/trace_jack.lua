-- Jack's position and speed per frame under scripted input (physics
-- measurements); driven by trace_jack.sh.
-- scenario file: first line = output path; then "start len field" lines (frames relative to T0)
local W=(os.getenv("BJTRACE") or ".").."/"
local spec={} local out
local fh=io.open(W.."scenario.txt") 
out=io.open(W..fh:read("l"),"w")
for l in fh:lines() do local s,n,fl=l:match("(%-?%d+)%s+(%d+)%s+(.+)") if s then spec[#spec+1]={tonumber(s),tonumber(n),fl} end end
fh:close()
local P=manager.machine.ioport.ports
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local function rb(a) return mem:read_u8(a) end
local function rw(a) return mem:read_u16(a) end
local T0=1950 local f=0
local fields={["Right"]=P[":P1"].fields["P1 Right"],["Left"]=P[":P1"].fields["P1 Left"],["Up"]=P[":P1"].fields["P1 Up"],["Down"]=P[":P1"].fields["P1 Down"],["Jump"]=P[":P1"].fields["P1 Button 1"]}
out:write("t,in,X,Y,E,vy,vx,flags,jt,col\n")
emu.register_frame_done(function()
  f=f+1
  P[":SYSTEM"].fields["Coin 1"]:set_value((f>=1500 and f<1506) and 1 or 0)
  P[":SYSTEM"].fields["1 Player Start"]:set_value((f>=1600 and f<1606) and 1 or 0)
  local t=f-T0 local act={}
  for k,v in pairs(fields) do v:set_value(0) end
  for _,s in ipairs(spec) do if t>=s[1] and t<s[1]+s[2] then fields[s[3]]:set_value(1) act[#act+1]=s[3] end end
  if t>=-5 and t<=400 then
    out:write(string.format("%d,%s,%d,%d,%d,%d,%d,%02x,%d,%02x\n",t,table.concat(act,"+"),
      rb(0x802b)*256+rb(0x802d), rb(0x802c)*256+rb(0x802e), rw(0x8042), (rw(0x8031)+32768)%65536-32768, (rw(0x802f)+32768)%65536-32768, rb(0x8028), rb(0x8033), rb(0x8041)))
  end
  if t==401 then out:close() manager.machine:exit() end
end)
