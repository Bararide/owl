#ifndef OWL_APPLICATION
#define OWL_APPLICATION

#include "vfs/fs/observer.hpp"

namespace owl {

class Application {
public:
    Application(const Application& other) = delete;
    Application(Application&& other) = delete;
    
    Application& operator=(const Application& other) = delete;
    Application& operator=(Application&& other) = delete;
    
    Application() : state_{} {}
    
private:
    State state_;
};

} // namespace owl

#endif // OWL_APPLICATION