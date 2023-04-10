# WXP_Webserver
一个轻量级c++实现的web服务器，根据游双的《liunx高性能服务器》改编而来
功能：
1、使用 线程池 + 非阻塞socket + epoll(ET和LT) + 事件处理(Reactor和模拟Proactor均实现) 实现并发
2、使用主从状态机解析HTTP请求报文，支持解析GET和POST请求
3、访问服务器数据库实现web端用户的各种功能（登陆、注册、下载、上传）
4、实现同步/异步日志系统，记录服务器运行状态