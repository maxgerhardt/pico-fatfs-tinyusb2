#include <Arduino.h>
#include <FatFS.h>
#include <Adafruit_TinyUSB.h>
#include "ff15/diskio.h"

#define FORCE_FATFS_FORMAT false

#if CFG_TUD_MSC_EP_BUFSIZE != 4096
#error "USB MSC buffer size does not equal 4096 bytes (Pico Flash erase size)."
#error "Modify C:\Users\<user>\.platformio\packages\framework-arduinopico\libraries\Adafruit_TinyUSB_Arduino\src\arduino\ports\rp2040\tusb_config_rp2040.h"
#endif

FATFS fatfs; /* instance from ff.h */
const char* err_to_str (FRESULT rc)
{
	const char *str =
		"OK\0" "DISK_ERR\0" "INT_ERR\0" "NOT_READY\0" "NO_FILE\0" "NO_PATH\0"
		"INVALID_NAME\0" "DENIED\0" "EXIST\0" "INVALID_OBJECT\0" "WRITE_PROTECTED\0"
		"INVALID_DRIVE\0" "NOT_ENABLED\0" "NO_FILE_SYSTEM\0" "MKFS_ABORTED\0" "TIMEOUT\0"
		"LOCKED\0" "NOT_ENOUGH_CORE\0" "TOO_MANY_OPEN_FILES\0" "INVALID_PARAMETER\0";
	int i;
	for (i = 0; i != (int) rc && *str; i++) {
		while (*str++) ;
	}
    return str;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
extern "C" int32_t my_tud_msc_read10_cb(uint32_t lba, void* buffer, uint32_t bufsize) {
    Serial.println("Read callback: bufsize is " + String(bufsize));
    disk_read_direct((BYTE*)buffer, lba, bufsize);
    return bufsize;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
extern "C" int32_t my_tud_msc_write10_cb(uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
    Serial.println("Write callback: bufsize is " + String(bufsize));
    // WITH MISCONFIGURED TINYUSB, bufsize IS ONLY 512 BYTES
    // BUT FF_MAX_SS = 4096
    // HENCE THE DIVISION GIVES A SECTOR COUNT OF 0.
    // We need to be able to support smaller buffer sizes too
    // we can do this here when CFG_TUD_MSC_EP_BUFSIZE == 4096
    disk_write(0, buffer, lba, bufsize/FF_MAX_SS);
    return bufsize;
}


Adafruit_USBD_MSC usb_msc;
void msc_flush_cb (void) { /* NOP */}
bool is_writable() { return true; }
void setup() {
    Serial.begin(115200);
    FRESULT res;

    res = f_mount(&fatfs, "", 1 /* force mount*/);    
    Serial.println("Mounting FatFS: " + String(err_to_str(res)));
    if(res == FR_NO_FILESYSTEM || FORCE_FATFS_FORMAT) {
        // format new filesystem since it doesn't exist yet
        const MKFS_PARM formatopt = {
            FM_ANY, 
            1, /* n_fat*/ 
            0, /* algin */ 
            0, /* n_root*/ 
            0  /* au_size */
        };
        static uint8_t work_buf[2*FF_MAX_SS];
        res = f_mkfs("0:", &formatopt, work_buf, sizeof(work_buf));
        Serial.println("Formatting filesystem: " + String(err_to_str(res)));
        // Also initialize it with a test file
        FIL fp {};
        res = f_open(&fp, "test.txt", FA_CREATE_ALWAYS | FA_WRITE);
        Serial.println("Opening File: " + String(err_to_str(res)));
        const char* content = "Hello from Pico FatFS";
        UINT bytes_written;
        res = f_write(&fp, content, strlen(content), &bytes_written);
        Serial.println("Writing File: " + String(err_to_str(res)) + " num bytes written: "  + String(bytes_written));
        res = f_close(&fp);
        Serial.println("Closing file File: " + String(err_to_str(res)) + " num bytes written: "  + String(bytes_written));
    }
    TCHAR labelBuf[64] = {};
    DWORD serNum;
    // Label not yet set?
    if(f_getlabel("", labelBuf, &serNum) == FR_OK && labelBuf[0] == '\0') {
        f_setlabel("Pico FatFS");
    }
#if 1
    usb_msc.setID("Pico", "Internal Flash", "1.0");
    DWORD sect_cnt = 0;
    disk_ioctl(0, GET_SECTOR_COUNT, &sect_cnt);
    usb_msc.setCapacity(sect_cnt, FF_MAX_SS);
    usb_msc.setReadWriteCallback(my_tud_msc_read10_cb, my_tud_msc_write10_cb, msc_flush_cb);
    usb_msc.setWritableCallback(is_writable);
    usb_msc.setUnitReady(true);
    bool ok = usb_msc.begin();
    Serial.println("MSC begin ok: " + String(ok));
#endif
}

void loop() {
    Serial.println("Hello");
    delay(1000);
}