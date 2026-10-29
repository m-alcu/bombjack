-- Start a game at the round in $BJTRACE/round.txt and dump its bombs and
-- video RAM (→ levels_data.h platforms). Writes bombs_rNN.txt, vram_rNN.bin.
local W=(os.getenv("BJTRACE") or ".").."/"
local P=manager.machine.ioport.ports
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local fh=io.open(W.."round.txt") local R=tonumber(fh:read("l")) fh:close()
local f=0
emu.register_frame_done(function()
  f=f+1
  P[":SYSTEM"].fields["Coin 1"]:set_value((f>=1500 and f<1506) and 1 or 0)
  P[":SYSTEM"].fields["1 Player Start"]:set_value((f>=1600 and f<1606) and 1 or 0)
  if f>=1590 and f<=1603 then mem:write_u8(0x81a0,R) end
  if f==1950 then
    local o=io.open(W..string.format("bombs_r%02d.txt",R),"w")
    for a=0x8480,0x84f8,5 do o:write(string.format("%d %d %d\n",mem:read_u8(a),mem:read_u8(a+1),mem:read_u8(a+2))) end o:close() o=io.open(W..string.format("vram_r%02d.bin",R),"wb") for a=0x9000,0x97ff do o:write(string.char(mem:read_u8(a))) end
    o:close()
    manager.machine.video:snapshot()
  end
  if f==1952 then manager.machine:exit() end
end)
