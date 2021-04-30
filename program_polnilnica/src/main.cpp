#include <mbed.h>

DigitalOut led(LED_RED);

Serial pc(USBTX, USBRX);
Serial hc_05(PA_11, PA_12);
// name: polnilnica
// pin: 1234

DigitalIn sw1(PC_14);
DigitalIn sw2(PC_15);
DigitalIn sw3(PH_0);
DigitalIn sw4(PH_1);

AnalogIn signalTemperature(PC_0);
AnalogIn signalContact(PA_1);

DigitalOut relayContact(PB_13);
DigitalOut relayHeater(PB_14);
DigitalOut relayActuator(PB_15);

Thread thread1;
Thread thread2;

// #define DEBUG 1;

void temperatureThread() {
    #ifdef DEBUG
        pc.baud(9600);
    #endif

    uint16_t sensorVal;
    float a = 283786.2;
    float b = 0.3; // 0.6593
    float c = 49886.0;

    bool isHeating = false;

    while(true) {
        sensorVal = signalTemperature.read_u16(); // 65536
        double temp = (-1 / b) * log((3.3 * 100000) / ((sensorVal * 3.3) / 65536 * a) - (c / a));

        #ifdef DEBUG
            pc.printf("Temperature: %f\n", temp);
        #endif

        if (temp < 0 && isHeating == false) {
            isHeating = true;
            relayHeater.write(true);
        } else if (temp > 40 && isHeating == true) {
            isHeating= false;
            relayHeater.write(false);
        }

        thread_sleep_for(1000); // 1s
    }
}

void mainThread() {
    hc_05.baud(9600);
    pc.baud(9600);

    char command[10];

    while(true) {
        if (hc_05.readable()) {
            hc_05.gets(command, 10);

            if(strcmp(command, "EXT-1234\n") == 0) {
                thread_sleep_for(500); // 0.5sec

                // mesature contact resistance
                uint16_t c_resistance = signalContact.read_u16();
                if (c_resistance > 60000) {
                    hc_05.printf("OK\n");
                    relayContact.write(true);
                } else {
                    hc_05.printf("ERR_C\n");
                }
            } else if(strcmp(command, "DONE\n") == 0) {
                relayContact.write(false);
            }
        }
    }
}

int main() {
    thread1.start(mainThread);
    thread2.start(temperatureThread);
}
