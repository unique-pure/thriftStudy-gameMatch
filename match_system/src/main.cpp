// This autogenerated skeleton file illustrates how to build a server.
// You should copy it to another filename to avoid overwriting it.

#include "match_server/Match.h"
#include "save_client/Save.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>

#include <iostream>
#include <mutex> // 锁的头文件
#include <thread> // 线程的头文件
#include <condition_variable> // 条件变量的头文件
#include <queue>
#include <vector>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using namespace  ::match_service;
using namespace  ::save_service;

using namespace std;

struct Task{
    User user; // 需要操作的对象
    string type; // 操作类型，是添加还是删除
};
// 消息队列
struct MessageQueue{
    // 队列是互斥的，同时只能有一个线程访问队列
    queue<Task> q;
    mutex m;
    condition_variable cv;
}message_queue;
// 匹配池类
class Pool{
private:
    vector<User> users; // 存储用户的池
public:
    void add(User user){
        // printf("%d\n", int(users.size()));
        users.push_back(user);
    }
    void remove(User user){
        // printf("%d\n", int(users.size()));
        for(unsigned int i = 0; i < users.size(); ++ i){
            if(users[i].id == user.id){
                users.erase(users.begin() + i);
                break;
            }
        }
    }
    void match(){
        while(users.size() > 1){
            // printf("%d\n", int(users.size()));
            auto user1 = users[0];
            auto user2 = users[1];
            users.erase(users.begin());
            users.erase(users.begin());
            save_result(user1.id, user2.id);
        }
    }
    void save_result(int id1, int id2){
        printf("success\n%d 和 %d 匹配成功！\n", id1, id2);

        // save_data
        std::shared_ptr<TTransport> socket(new TSocket("123.57.47.211", 9090));
        std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
        std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
        SaveClient client(protocol);
        try {
            transport->open();
            // 在此之间实现自己的业务
            client.save_data("acs_878", "461ff549", id1, id2);

            // ----------------
            cout << "数据保存成功！" << endl;
            transport->close();
        } catch(TException &e) {
            cout << "ERROR:" << e.what() << endl;
        }
    }
}pool;
class MatchHandler : virtual public MatchIf {
 public:
  MatchHandler() {
    // Your initialization goes here
  }

  /**
   * user：添加的用户信息
   * info：附加信息
   * 作用：向匹配池中添加一名用户
   * 
   * 
   * @param user
   * @param info
   */
  int32_t add_user(const User& user, const std::string& info) {
    // Your implementation goes here
    printf("add_user\n");

    unique_lock<mutex> lock(message_queue.m); // 加锁
    message_queue.q.push({user, "add"});
    message_queue.cv.notify_all(); // 当有操作的时候，应该唤醒线程

    return 0;
  }

  /**
   * user：删除的用户信息
   * info：附加信息
   * 作用：向匹配池中删除一名用户
   * 
   * 
   * @param user
   * @param info
   */
  int32_t remove_user(const User& user, const std::string& info) {
    // Your implementation goes here
    printf("remove_user\n");
    unique_lock<mutex> lock(message_queue.m); // 加锁
    message_queue.q.push({user, "remove"});
    message_queue.cv.notify_all(); // 当有操作的时候，应该唤醒线程
    return 0;
  }
};
// 线程操作的函数
void consume_task(){
    while(true){
        unique_lock<mutex> lock(message_queue.m); // 加锁
        if(message_queue.q.empty()){
            // 如果为空，直接continue不作处理则一定会死循环。
            message_queue.cv.wait(lock);
        } else{
            auto task = message_queue.q.front();
            message_queue.q.pop();
            // 因为只有队列是互斥的，为了保证程序的快速运行，这里需要释放锁。
            lock.unlock();
            if(task.type == "add"){
                pool.add(task.user);
            } else if(task.type == "remove"){
                pool.remove(task.user);
            }
            pool.match();
        }
    }
}

int main(int argc, char **argv) {
  int port = 9090;
  ::std::shared_ptr<MatchHandler> handler(new MatchHandler());
  ::std::shared_ptr<TProcessor> processor(new MatchProcessor(handler));
  ::std::shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
  ::std::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
  ::std::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

  TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);
  printf("Match server start!\n");
  thread matching_thread(consume_task);
  server.serve();
  return 0;
}

