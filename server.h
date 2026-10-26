#ifndef SERVER_H
#define SERVER_H

#include <iostream>

// Is there a more generic sim? Probably not
class Server {
private:
    // a data structure of all NPC entities
    // History, what does that look like? Per networked entity, right? So a vector of all
    // of them?
    // The map and all its data.
    // validation variables
    // networking 
    // client ref, shared ptr
public:
    virtual ~Server() = default;
    // update
};

#endif // SERVER_H
