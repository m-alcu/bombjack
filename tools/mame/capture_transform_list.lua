-- The mummy transform list (RAM 0x8268) of round $BJROUND. Writes tlist_NN.txt.
local W=(os.getenv("BJTRACE") or ".").."/"
local P=manager.machine.ioport.ports
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local R=tonumber(os.getenv("BJROUND"))
local f=0
emu.register_frame_done(function()
  f=f+1
  P[":SYSTEM"].fields["Coin 1"]:set_value((f>=1500 and f<1506) and 1 or 0)
  P[":SYSTEM"].fields["1 Player Start"]:set_value((f>=1600 and f<1606) and 1 or 0)
  if f>=1590 and f<=1603 then mem:write_u8(0x81a0,R) end
  if f==1960 then
    local s={} for a=0x8268,0x826f do s[#s+1]=string.format("%02x",mem:read_u8(a)) end
    local o=io.open(W..string.format("tlist_%02d.txt",R),"w") o:write(R.." "..table.concat(s," ").."\n") o:close()
    manager.machine:exit()
  end
end)
