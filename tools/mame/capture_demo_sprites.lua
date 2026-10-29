-- The sprite list (RAM 0x8500) every demo frame (→ demo_runs.h).
-- Writes $BJTRACE/demosprites.txt.
local W=(os.getenv("BJTRACE") or ".").."/"
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local f=0
local out=io.open(W.."demosprites.txt","w")
emu.register_frame_done(function()
  f=f+1
  local idx=mem:read_u8(0x8079)
  if idx==1 or idx==3 then
    local t={string.format("%d %d %d %04x %d %d %d %02x |",f,idx,mem:read_u8(0x80e8),mem:read_u16(0x807c),mem:read_u8(0x802b),mem:read_u8(0x802c),mem:read_u8(0x8078),mem:read_u8(0x8028))}
    for a=0x8500,0x855c,4 do
      local c=mem:read_u8(a)
      local x,y=mem:read_u8(a+2),mem:read_u8(a+3)
      if c~=0 or x~=0 or y~=0 then t[#t+1]=string.format(" %d:%02x,%02x,%d,%d",(a-0x8500)//4,c,mem:read_u8(a+1),x,y) end
    end
    out:write(table.concat(t).."\n")
  end
  if f==130000 then out:close() manager.machine:exit() end
end)
