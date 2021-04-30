#include <mbed.h>
#include <math.h>

DigitalOut led(LED_RED);

Serial pc(USBTX, USBRX);
Serial hc_06(PA_11, PA_12);
// name: bus
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

// uncomment for debugging
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

    while (true) {
        sensorVal = signalTemperature.read_u16(); // 65536
        double temp = (-1 / b) * log((3.3 * 100000) / ((sensorVal * 3.3) / 65536 * a) - (c / a));

        #ifdef DEBUG
            pc.printf("Temperature: %f\n", temp);
        #endif

        if (temp < 0 && isHeating == false){
            isHeating = true;
            relayHeater.write(true);
        }
        else if (temp > 40 && isHeating == true) {
            isHeating = false;
            relayHeater.write(false);
        }

        thread_sleep_for(1000); // 1s
    }
}

void mainThread() {
    hc_06.baud(9600);
    pc.baud(9600);

    // hc_06.printf("AT+UART=9600,0,0\r\n");
    // hc_06.printf("AT+CMODE=1\r\n");
    // hc_06.printf("AT+ROLE=1\r\n");

    bool doRun = false;

    while (true) {
        if (doRun) {
            led.write(1);

            // resistance before extending
            uint16_t c_resistance_1 = signalContact.read_u16();
            if (c_resistance_1 > 60000) {

                // extend actuator out
                relayActuator.write(true);
                // wait actuator to extend all the way out
                thread_sleep_for(2000);

                // resistance after extending
                uint16_t c_resistance_2 = signalContact.read_u16();

                #ifdef DEBUG
                    printf("Before: %d --- After: %d", c_resistance_1, c_resistance_2);
                #endif

                if (c_resistance_2 > 60000) {
                    hc_06.printf("EXT-1234\n"); // send bluetooth signal - contact status: EXT-id (extended)

                    char msgBack[10];
                    bool receivedMessage = false;
                    uint64_t starTime = us_ticker_read() / 1000;
                    uint64_t currentTime = 0;

                    //loop untill it receives message or runs out of time
                    while (!receivedMessage && (starTime + 5000 > currentTime)) { // 5sec max
                        currentTime = us_ticker_read() / 1000;
                        if (hc_06.readable()) {
                            hc_06.gets(msgBack, 10);
                            #ifdef DEBUG
                                pc.putc(hc_06.getc());
                            #endif
                            receivedMessage = true;
                        }
                    }

                    #ifdef DEBUG
                        pc.printf("Message received: %s", msgBack);
                    #endif

                    if (strcmp(msgBack, "OK\n") == 0) { // if return message is "READY"
                        relayContact.write(true);

                        bool isDone = false;
                        while (!isDone) {
                            int switchVal = sw4.read();

                            if (switchVal == 0) { // pullup switch
                                // done with charging
                                hc_06.printf("DONE\n"); // send bluetooth signal - contact status: DONE (done)

                                relayActuator.write(false); // contract contact
                                thread_sleep_for(2000);     // wait 2s
                                relayContact.write(false);

                                isDone = true;

                                #ifdef DEBUG
                                    printf("Done with charging");
                                #endif
                            }
                        }
                    } else {
                        #ifdef DEBUG
                            printf("Error - Didnt receive READY message");
                        #endif
                    }
                } else {
                    #ifdef DEBUG
                        printf("Contact resistance Error");
                    #endif
                }
            } else {
                #ifdef DEBUG
                    printf("Contact resistance Error");
                #endif
            }

            led.write(0);
            doRun = false;
        } else {
            // if system is not running, assing variable new value that is read from sw4
            // negate read from sw4 because it works in pull-up mode
            if (!doRun) {
                doRun = !sw4.read();
            }
        }
    }
}

int main() {
    thread1.start(mainThread);
    thread2.start(temperatureThread);

    hc_06.baud(9600);
    pc.baud(9600);
}
