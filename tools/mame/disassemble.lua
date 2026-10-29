-- Disassemble the main CPU ROM into full_0000.txt / full_c000.txt
-- (run with -debug -debugger none).
local W=(os.getenv("BJTRACE") or ".").."/"
local f=0
emu.register_frame_done(function()
  f=f+1
  if f==2 then
    local d=manager.machine.debugger
    d:command("dasm "..W.."full_0000.txt,0x0000,0x8000,0")
    d:command("dasm "..W.."full_c000.txt,0xc000,0x2000,0")
    manager.machine:exit()
  end
end)
