#include "mbed.h"
#include "LITE_CDH.h"
#include "LITE_EPS.h"
#include "LITE_SENSOR.h"
#include "BME280.h"

// I2Cピン定義 (NUCLEO L432KCの場合)
//#define SDA_PIN PB_7
//#define SCL_PIN PB_6

// I2Cインタフェース作成
//I2C i2c(SDA_PIN, SCL_PIN);

// BME280インスタンスをI2Cで作成 (アドレスは0x76)
BME280 tenkisensor(PB_7, PB_6); // アドレスを左シフトして8bitにする

LITE_CDH cdh(PB_5, PB_4, PB_3, PA_8, "sd", PA_3);
LITE_EPS    eps(PA_0, PA_4);
LITE_SENSOR sensor(PA_7, PB_7, PB_6);
RawSerial   pc(USBTX, USBRX, 9600);
Ticker      logger;
//DigitalOut  indicator(LED1);   // サンプリング時に点灯

// ログ間隔（秒）
static const float LOG_INTERVAL = 1.0f;

// 日付時刻入力用バッファ
char time_buf[32];

// 1行分読み込んでバッファに格納。改行文字は含まず、NULL終端する
int readline(RawSerial &ser, char *buf, int buflen) {
    int idx = 0;
    while (true) {
        if (!ser.readable()) continue;
        char c = ser.getc();
        if (c == '\r' || c == '\n') break;
        if (idx < buflen - 1) buf[idx++] = c;
    }
    buf[idx] = '\0';
    return idx;
}

void init_time() {
    pc.printf("Enter date/time as: YYYY MM DD hh mm ss\r\n");
    // 例入力: 2025 05 01 14 30 00
    int len = readline(pc, time_buf, sizeof(time_buf));
    int y, mo, d, h, mi, s;
    if (len > 0 && sscanf(time_buf, "%d %d %d %d %d %d",
                        &y, &mo, &d, &h, &mi, &s) == 6) {
        struct tm t = {0};
        t.tm_year = y - 1900;
        t.tm_mon  = mo - 1;
        t.tm_mday = d;
        t.tm_hour = h;
        t.tm_min  = mi;
        t.tm_sec  = s;
        time_t epoch = mktime(&t);
        set_time(epoch);
        pc.printf("RTC set: %04d/%02d/%02d %02d:%02d:%02d\r\n",
                  y, mo, d, h, mi, s);
    } else {
        pc.printf("Invalid format, keeping default time\r\n");
    }
}

void log_task() {
    // 1) インジケータ ON + センサ起動
    //indicator = 1;
    eps.turn_on_regulator();
    wait(0.01f);
    
    // 2) データ取得
    pc.printf("a");
    float batvol, temp;
    eps.vol(&batvol);
    sensor.temp_sense(&temp);

    float tenkitemp = tenkisensor.getTemperature();
    float humidity = tenkisensor.getHumidity();
    float pressure = tenkisensor.getPressure();

    // 3) タイムスタンプ
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    // 4) ファイル追記（mount/unmount は不要）
    FILE *fp = fopen("/sd/mydir/data_01.csv", "a");
    if (fp) {
        fprintf(fp,
            "%04d-%02d-%02D %02d:%02d:%02d,%.3f,%.2f,%.2f,%.2f,%.2f\r\n",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec,
            batvol, temp,tenkitemp,humidity, pressure
        );
        fclose(fp);
    } else {
        pc.printf("File open error\r\n");
    }

    // 5) シリアルログ
    pc.printf("%02d:%02d:%02d  V=%.3fV  T=%.2f°C TT=%.2f H=%.2f P=%.2f\r\n",
        tm->tm_hour, tm->tm_min, tm->tm_sec,
        batvol, temp,tenkitemp,humidity, pressure
    );

    // 6) センサ停止 + インジケータ OFF
    eps.shut_down_regulator();
    //indicator = 0;
}

int main() {
    pc.printf("Start logging every %.0f s\r\n", LOG_INTERVAL);
    //init_time();            //起動時に時刻を設定
    eps.turn_on_regulator();
    wait(1.01f);

    // ディレクトリとヘッダの準備（初回のみ）
    mkdir("/sd/mydir", 0777);
    FILE *hf = fopen("/sd/mydir/data_01.csv", "r");
    if (!hf) {
        hf = fopen("/sd/mydir/data_01.csv", "w");
        fprintf(hf, "timestamp,battery_V,temperature_C,tenkitemp,humidity,pressure\r\n");
    }
    if (hf) fclose(hf);

    // 初回ログ実行
    //log_task();

    // 定期実行セット
    //logger.attach(&log_task, LOG_INTERVAL);

    // 低消費モードで待機
    //while (1) {
       // sleep();
    //}
    while(1){
        
        
        // 2) データ取得
        
        float batvol, temp,tenkitemp;
        eps.vol(&batvol);
        sensor.temp_sense(&temp);
        float mx,my,mz;
        sensor.set_up();
        sensor.sen_acc(&mx,&my,&mz);
        tenkisensor.initialize();
        pc.printf("mag : %f,%f,%f\r\n",mx,my,mz);
        tenkitemp = tenkisensor.getTemperature();
        float humidity = tenkisensor.getHumidity();
        float pressure = tenkisensor.getPressure();

        // 3) タイムスタンプ
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);

        // 4) ファイル追記（mount/unmount は不要）
        FILE *fp = fopen("/sd/mydir/data_01.csv", "a");
        if (fp) {
            fprintf(fp,
                "%04d-%02d-%02D %02d:%02d:%02d,%.3f,%.2f,%.2f,%.2f,%.2f\r\n",
                tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                tm->tm_hour, tm->tm_min, tm->tm_sec,
                batvol, temp,tenkitemp,humidity, pressure
            );
            fclose(fp);
        } else {
            pc.printf("File open error\r\n");
        }

        // 5) シリアルログ
        pc.printf("%02d:%02d:%02d  V=%.3fV  T=%.2f°C TT=%.2f H=%.2f P=%.2f\r\n",
            tm->tm_hour, tm->tm_min, tm->tm_sec,
            batvol, temp,tenkitemp,humidity, pressure
        );
        wait(1);//検証のために1秒間隔
        // 6) センサ停止 + インジケータ OFF
        //eps.shut_down_regulator();
        //sleep();
    }
}
