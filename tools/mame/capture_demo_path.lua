-- Jack's path during the attract demos, unhittable (→ demo_data.h tracks).
-- Writes $BJTRACE/demotraj.csv.
local W=(os.getenv("BJTRACE") or ".").."/"
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local f=0
local out=io.open(W.."demotraj.csv","w")
out:write("f,idx,rnd,e8,ptr,cnt,inp,x,xf,y,yf,st\n")
-- Jack can't be hit: drop the hit flag (bit 2 of 0x8028) whenever it is written
T1=mem:install_write_tap(0x8028,0x8028,"inv",function(off,data,mask)
  if (data & 4) ~= 0 then return data & 0xfb end
end)
emu.register_frame_done(function()
  f=f+1
  local idx=mem:read_u8(0x8079)
  if idx==1 or idx==3 then
    out:write(string.format("%d,%d,%d,%d,%04x,%d,%d,%d,%d,%d,%d,%02x\n",f,idx,mem:read_u8(0x81a0),mem:read_u8(0x80e8),
      mem:read_u16(0x807c),mem:read_u8(0x807b),mem:read_u8(0x807a),mem:read_u8(0x802b),mem:read_u8(0x802d),mem:read_u8(0x802c),mem:read_u8(0x802e),mem:read_u8(0x8028)))
  end
  if f==14000 then out:close() manager.machine:exit() end
end)
