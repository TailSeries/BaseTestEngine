schtasks /create /tn "CodexSourceWatcher" /tr "powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "F:\workspace\BaseTestEngine\check-source.ps1"" /sc MINUTE /mo 5 /f
