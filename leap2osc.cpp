#include <iostream>
#include "osc/OscOutboundPacketStream.h"
#include "ip/UdpSocket.h"

#include "Leap.h"

#define OUTPUT_BUFFER_SIZE 8192

using namespace Leap;
class OSCFeeder : public Listener {
    UdpTransmitSocket * transmitSocket;
public:
    virtual void setSocket(UdpTransmitSocket&);
    virtual void simpleStatus(const char*);
    virtual void onInit(const Controller&);
    virtual void onConnect(const Controller&);
    virtual void onDisconnect(const Controller&);
    virtual void onExit(const Controller&);
    virtual void onFrame(const Controller&);
    virtual void onFocusGained(const Controller&);
    virtual void onFocusLost(const Controller&);
    virtual void onDeviceChange(const Controller&);
    virtual void onServiceConnect(const Controller&);
    virtual void onServiceDisconnect(const Controller&); };

const std::string fingerNames[] = {"thumb", "index", "middle", "ring", "pinky"};
const std::string boneNames[] = {"metacarpal", "proximal", "middle", "distal"};
const std::string stateNames[] = {"invalid", "start", "update", "end"};

void OSCFeeder::setSocket(UdpTransmitSocket& s) { transmitSocket = &s; }

void OSCFeeder::simpleStatus(const char* s) {
    std::cout << "/" << s << std::endl;
    char buffer[OUTPUT_BUFFER_SIZE];
    osc::OutboundPacketStream p( buffer, OUTPUT_BUFFER_SIZE );
    p << osc::BeginBundleImmediate << osc::BeginMessage(s) << osc::EndMessage << osc::EndBundle;
    transmitSocket->Send( p.Data(), p.Size() ); }

void OSCFeeder::onInit(const Controller& controller) {
    this->simpleStatus("init"); }

void OSCFeeder::onConnect(const Controller& controller) {
    this->simpleStatus("connect");
    controller.enableGesture(Gesture::TYPE_CIRCLE);
    controller.enableGesture(Gesture::TYPE_KEY_TAP);
    controller.enableGesture(Gesture::TYPE_SCREEN_TAP);
    controller.enableGesture(Gesture::TYPE_SWIPE); }

void OSCFeeder::onDisconnect(const Controller& controller) { this->simpleStatus("disconnect"); }

void OSCFeeder::onExit(const Controller& controller) { this->simpleStatus("exit"); }

void OSCFeeder::onFrame(const Controller& controller) {
    char buffer[OUTPUT_BUFFER_SIZE];
    osc::OutboundPacketStream p( buffer, OUTPUT_BUFFER_SIZE );
    p << osc::BeginBundleImmediate;

    const Frame frame = controller.frame();
    p << osc::BeginMessage("/frame") << frame.id() << frame.timestamp()
      << frame.hands().count() << frame.fingers().count() << frame.tools().count() << frame.gestures().count() << osc::EndMessage;

    HandList hands = frame.hands();
    for (HandList::const_iterator hl = hands.begin(); hl != hands.end(); ++hl) {
        const Hand hand = *hl;
        std::string handType = hand.isLeft() ? "left" : "right";
        const Vector normal = hand.palmNormal();
        const Vector direction = hand.direction();
        const Vector position = hand.palmPosition();

        p << osc::BeginMessage("/hand")
          << handType.c_str()
          << hand.id()
          << position.x << position.y << position.z
          << direction.pitch() << normal.roll() << direction.yaw()
          << osc::EndMessage;

        // Get fingers
        const FingerList fingers = hand.fingers();
        for (FingerList::const_iterator fl = fingers.begin(); fl != fingers.end(); ++fl) {
            const Finger finger = *fl;
            p << osc::BeginMessage("/finger") << handType.c_str() << hand.id()
              << fingerNames[finger.type()].c_str() << finger.id() << finger.length() << finger.width()
              << osc::EndMessage;
            // Get finger bones
            for (int b = 0; b < 4; ++b) {
                Bone::Type boneType = static_cast<Bone::Type>(b);
                Bone bone = finger.bone(boneType);

                const Vector prev = bone.prevJoint();
                const Vector next = bone.nextJoint();
                const Vector direction = bone.direction();

                p << osc::BeginMessage("/bone") << handType.c_str() << hand.id()
                  << fingerNames[finger.type()].c_str() << finger.id()
                  << boneNames[boneType].c_str()
                  << prev.x << prev.y << prev.z
                  << next.x << next.y << next.z
                  << direction.pitch() << direction.roll() << direction.yaw()
                  << osc::EndMessage; } } }

    // Get tools
    const ToolList tools = frame.tools();
    for (ToolList::const_iterator tl = tools.begin(); tl != tools.end(); ++tl) {
        const Tool tool = *tl;
        const Vector position = tool.tipPosition();
        const Vector direction = tool.direction();
        
        p << osc::BeginMessage("/tool") << tool.id()
          << position.x << position.y << position.z
          << direction.pitch() << direction.roll() << direction.yaw()
          << osc::EndMessage; }

    // Get gestures
    const GestureList gestures = frame.gestures();
    for (int g = 0; g < gestures.count(); ++g) {
        Gesture gesture = gestures[g];
        switch (gesture.type()) {
        case Gesture::TYPE_CIRCLE: {
            CircleGesture circle = gesture;
            std::string clockwiseness;
            
            if (circle.pointable().direction().angleTo(circle.normal()) <= PI/4) {
                clockwiseness = "clockwise";
            } else { clockwiseness = "counterclockwise"; }
            
            // Calculate angle swept since last frame
            float sweptAngle = 0;
            if (circle.state() != Gesture::STATE_START) {
                CircleGesture previousUpdate = CircleGesture(controller.frame(1).gesture(circle.id()));
                sweptAngle = (circle.progress() - previousUpdate.progress()) * 2 * PI; }
            
            p << osc::BeginMessage("/gesture/circle") << gesture.id()
              << stateNames[gesture.state()].c_str()
              << circle.progress()
              << circle.radius()
              << sweptAngle
              << clockwiseness.c_str()
              << osc::EndMessage;
            break; }
        case Gesture::TYPE_SWIPE: {
            SwipeGesture swipe = gesture;
            Vector direction = swipe.direction();

            p << osc::BeginMessage("/gesture/swipe") << gesture.id()
              << stateNames[gesture.state()].c_str()
              << direction.pitch() << direction.roll() << direction.yaw()
              << swipe.speed()
              << osc::EndMessage;
            break; }
        case Gesture::TYPE_KEY_TAP: {
            KeyTapGesture tap = gesture;
            Vector position = tap.position();
            Vector direction = tap.direction();

            p << osc::BeginMessage("/gesture/keytap") << gesture.id()
              << stateNames[gesture.state()].c_str()
              << position.x << position.y << position.z
              << direction.pitch() << direction.roll() << direction.yaw()
              << osc::EndMessage;
            break; }
        case Gesture::TYPE_SCREEN_TAP: {
            ScreenTapGesture tap = gesture;
            Vector position = tap.position();
            Vector direction = tap.direction();

            p << osc::BeginMessage("/gesture/screentap") << gesture.id()
              << stateNames[gesture.state()].c_str()
              << position.x << position.y << position.z
              << direction.pitch() << direction.roll() << direction.yaw()
              << osc::EndMessage;
            break; }
        default:
            std::cout << "got unknown gesture" << std::endl;
            break;
        } }

    p << osc::EndBundle;

    transmitSocket->Send( p.Data(), p.Size() ); }

void OSCFeeder::onFocusGained(const Controller& controller) { this->simpleStatus("focus/gain"); }

void OSCFeeder::onFocusLost(const Controller& controller) { this->simpleStatus("focus/lost"); }

void OSCFeeder::onDeviceChange(const Controller& controller) {
    char buffer[OUTPUT_BUFFER_SIZE];
    osc::OutboundPacketStream p( buffer, OUTPUT_BUFFER_SIZE );
    p << osc::BeginBundleImmediate << osc::BeginMessage("/device_changed") << osc::EndMessage;
    std::cout << "device changed" << std::endl;
    const DeviceList devices = controller.devices();

    for (int i = 0; i < devices.count(); ++i) {
        p << osc::BeginMessage("/device") << devices[i].toString().c_str() << (devices[i].isStreaming() ? "true" : "false") << osc::EndMessage;
        std::cout << "id: " << devices[i].toString() << std::endl;
        std::cout << "  is_streaming: " << (devices[i].isStreaming() ? "true" : "false") << std::endl; }
    p << osc::EndBundle;
    transmitSocket->Send( p.Data(), p.Size() ); }

void OSCFeeder::onServiceConnect(const Controller& controller) { this->simpleStatus("service/connect"); }

void OSCFeeder::onServiceDisconnect(const Controller& controller) { this->simpleStatus("service/diconnect"); }

int main(int argc, char *argv[]) {
    char * host = argv[1];
    int port = (int)strtol(argv[2],NULL,10);
    std::cout << "osc stream to " << host << ":" << port << std::endl;
    UdpTransmitSocket transmitSocket( IpEndpointName( host, port ) );
    OSCFeeder feeder;
    Controller controller;
    feeder.setSocket(transmitSocket);
    controller.addListener(feeder);
    std::cout << "press enter to quit..." << std::endl;
    std::cin.get();
    controller.removeListener(feeder);
    return 0; }
