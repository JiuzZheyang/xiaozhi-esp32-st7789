@echo off
REM ============================================================
REM 小智 ESP32-S3 编译烧录脚本 (v2.2.4)
REM 使用 E:\Espressif ESP-IDF 工具
REM 板型: my-s3-st7789-154 (ST7789 1.54寸屏幕)
REM ============================================================

cd /d %~dp0

setlocal enabledelayedexpansion

set "IDF_PATH=E:\Espressif\frameworks\esp-idf-v5.5.3"
set "IDF_PYTHON_ENV_PATH=E:\Espressif\python_env\idf5.5_py3.11_env"
set "IDF_TOOLS_PATH=D:\Espressif"
set "IDF_SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.defaults.esp32s3"

set "PATH=D:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20251107\xtensa-esp-elf\bin;D:\Espressif\tools\idf-git\2.39.2\cmd;D:\Espressif\tools\cmake\3.30.2\bin;D:\Espressif\tools\ninja\1.12.1;%PATH%"

set "PYTHON=%IDF_PYTHON_ENV_PATH%\Scripts\python.exe"

echo [INFO] 小智 ESP32-S3 v2.2.4 编译烧录
echo [INFO] 板型: my-s3-st7789-154
echo [INFO] ESP-IDF: %IDF_PATH%
echo.

echo [1/4] 清理旧构建...
if exist build (
    rmdir /s /q build
)
echo.

echo [2/4] 设置目标为 esp32s3...
%PYTHON% "%IDF_PATH%\tools\idf.py" set-target esp32s3
if errorlevel 1 (
    echo [ERROR] 设置目标失败！
    pause
    exit /b 1
)
echo.

echo [3/4] 选择板型: my-s3-st7789-154
%PYTHON% "%IDF_PATH%\tools\idf.py" reconfigure --ESPKIT_BOARD=my-s3-st7789-154
if errorlevel 1 (
    echo [ERROR] 选择板型失败！
    pause
    exit /b 1
)
echo.

echo [4/4] 编译项目...
%PYTHON% "%IDF_PATH%\tools\idf.py" build
if errorlevel 1 (
    echo [ERROR] 编译失败！
    pause
    exit /b 1
)
echo.

echo ========================================================
echo 编译成功！
echo 请确保按住 BOOT 键并重新插入 USB 线以进入下载模式
echo ========================================================
echo.

echo 正在烧录到 ESP32-S3 (COM3)...
%PYTHON% "%IDF_PATH%\tools\idf.py" -p COM3 flash
if errorlevel 1 (
    echo [ERROR] 烧录失败！
    pause
    exit /b 1
)

echo.
echo ========================================================
echo 烧录完成！
echo 请按 RST 键重启 ESP32-S3
echo ========================================================
pause
