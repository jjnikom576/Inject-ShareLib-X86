# NDK Build Instructions
This guide provides step-by-step instructions for building Android NDK libraries supporting x86 and x86_64 architectures.

## Prerequisites
- Android NDK (Download from [Android NDK Archive](https://github.com/android/ndk/wiki/Unsupported-Downloads))
    - Recommended version: r25c (fastest compilation time)
- Command-line interface (Terminal/CMD)
- ADB (Android Debug Bridge)

## Setup Instructions

### 1. NDK Installation
1. Download the Android NDK package from the provided link
2. Extract the downloaded package to your C: drive or preferred location
3. Add the NDK installation path to your system's Environment Variables

### 2. Build Process
1. Navigate to your project directory using Command Prompt/Terminal
2. Execute the following commands:

#### Clean Build Output
```bash
ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk NDK_APPLICATION_MK=./Application.mk clean
```

#### Build Library
```bash
ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk NDK_APPLICATION_MK=./Application.mk
```

## Usage Instructions

### 1. Push Injector
Upload the executable library to the device:
```bash
adb push libs/x86/injector /data/local/tmp/injector
```

### 2. Set Permissions
Grant execution permissions to the injector:
```bash
adb shell "chmod 755 /data/local/tmp/injector"
```

### 3. Get Target PID
Find the process ID of your target application:
```bash
adb shell ps
```

### 4. Inject Library
Execute the injector with the target PID:
```bash
adb shell "/data/local/tmp/injector <pid> /data/local/tmp/libImGUI.so"
```

### 5. Monitor Logs
Watch the injection process through ADB logs:
```bash
adb logcat -s Injector:*
```

## Important Notes
- This build configuration only supports x86 and x86_64 architectures
- For additional issues or troubleshooting, please research the error messages or create an issue in this repository

---

# คำแนะนำการ Build NDK
คู่มือนี้จะแนะนำขั้นตอนการ build ไลบรารี Android NDK สำหรับสถาปัตยกรรม x86 และ x86_64

## สิ่งที่ต้องมี
- Android NDK (ดาวน์โหลดจาก [Android NDK Archive](https://github.com/android/ndk/wiki/Unsupported-Downloads))
    - แนะนำเวอร์ชัน: r25c (ใช้เวลาคอมไพล์เร็วที่สุด)
- Command-line interface (Terminal/CMD)
- ADB (Android Debug Bridge)

## ขั้นตอนการติดตั้ง

### 1. การติดตั้ง NDK
1. ดาวน์โหลดแพ็คเกจ Android NDK จากลิงก์ที่ให้ไว้
2. แตกไฟล์ที่ดาวน์โหลดไปยังไดรฟ์ C: หรือตำแหน่งที่ต้องการ
3. เพิ่มพาธการติดตั้ง NDK ในตัวแปรสภาพแวดล้อมของระบบ (Environment Variables)

### 2. ขั้นตอนการ Build
1. เปิด Command Prompt/Terminal แล้วนำทางไปยังไดเรกทอรีของโปรเจค
2. รันคำสั่งต่อไปนี้:

#### ล้างข้อมูล Build เก่า
```bash
ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk NDK_APPLICATION_MK=./Application.mk clean
```

#### Build ไลบรารี
```bash
ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk NDK_APPLICATION_MK=./Application.mk
```

## วิธีการใช้งาน

### 1. อัพโหลด Injector
อัพโหลดไลบรารีที่ compile แล้วไปยังอุปกรณ์:
```bash
adb push libs/x86/injector /data/local/tmp/injector
```

### 2. ตั้งค่าสิทธิ์การเข้าถึง
กำหนดสิทธิ์การเข้าถึงให้กับ injector:
```bash
adb shell "chmod 755 /data/local/tmp/injector"
```

### 3. ค้นหา PID เป้าหมาย
ค้นหารหัสกระบวนการ (PID) ของแอปพลิเคชันเป้าหมาย:
```bash
adb shell ps
```

### 4. ทำการ Inject ไลบรารี
รันคำสั่ง inject พร้อมระบุ PID เป้าหมาย:
```bash
adb shell "/data/local/tmp/injector <pid> /data/local/tmp/libImGUI.so"
```

### 5. ตรวจสอบบันทึก
ดูบันทึกการทำงานผ่าน ADB logs:
```bash
adb logcat -s Injector:*
```

## หมายเหตุสำคัญ
- การตั้งค่าการ build นี้รองรับเฉพาะสถาปัตยกรรม x86 และ x86_64 เท่านั้น
- หากพบปัญหาเพิ่มเติม กรุณาค้นคว้าจากข้อความผิดพลาดที่แสดง หรือสร้างปัญหาใหม่ในที่เก็บนี้
