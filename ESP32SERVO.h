#ifndef ESP32SERVO_H
#define ESP32SERVO_H

class ESP32SERVO {
public:
    ESP32SERVO(void) {};
    virtual ~ESP32SERVO(void) {};
    int begin(int pin, int ch);
    void write(int angle);
    void move(int);
private:
    int _ch;
    int _angle;
    int _min = 26; // (26/1024)*20ms = 0.5 ms (-90)
    int _max = 123; // (123/1024)*20ms = 2.4 ms (90)
};

#endif // ESP32SERVO_H
