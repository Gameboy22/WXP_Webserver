#include "lst_timer.h"
#include "../http_conn/http_conn.h"

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;


class Utils;
void cb_func(client_data *user_data)
{
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}
