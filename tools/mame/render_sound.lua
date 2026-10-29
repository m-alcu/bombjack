-- Send one sound command and record it; driven by render_sound.sh.
local W=(os.getenv("BJTRACE") or ".").."/"
local mem=manager.machine.devices[":maincpu"].spaces["program"]
local fh=io.open(W.."cmd.txt") local CMD=tonumber(fh:read("l"),16) local LEN=tonumber(fh:read("l")) fh:close()
local f=0
local mine=false
-- after the injection, swallow every main-CPU write to the sound latch (0 = no command)
T1=mem:install_write_tap(0xb800,0xb800,"blk",function(off,data,mask)
  if f>=590 and not mine then return 0 end
end)
emu.register_frame_done(function()
  f=f+1
  if f==600 then mine=true mem:write_u8(0xb800,CMD) mine=false end
  if f==600+LEN then manager.machine:exit() end
end)
