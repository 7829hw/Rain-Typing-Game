#ifndef NETWORK_SERVER_H
#define NETWORK_SERVER_H

// handle_client 함수의 프로토타입(선언)
// 이 함수는 network_server.c 에 정의될 것입니다.
// main_server.c 에서는 이 선언을 보고 함수를 호출할 수 있게 됩니다.
void* handle_client(void* arg);

#endif  // NETWORK_SERVER_H