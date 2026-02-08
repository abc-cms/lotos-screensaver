#pragma once

class saver_t {
public:
    virtual ~saver_t(){};
    virtual void reset() = 0;
    virtual void run() = 0;
};
