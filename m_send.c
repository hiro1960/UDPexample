#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

/*
 *  マルチキャスト情報
 */
struct multicast_info {
  unsigned short port;     /* ポート番号 */
  char *ipaddr;            /* マルチキャストのIPアドレス */
  char *msg;               /* 送信メッセージ */
  unsigned int msg_len;    /* メッセージ長 */

  int sd;                  /* ソケットディスクリプタ */
  struct sockaddr_in addr; /* マルチキャストのアドレス構造体 */
  unsigned char ttl;       /* Time to Live */
};
typedef struct multicast_info mc_info_t;

/*!
 * @brief      マルチキャスト機能を有効にする
 * @param[in]  info   マルチキャスト情報
 * @param[out] errmsg エラーメッセージ格納先
 * @return     成功ならば0、失敗ならば-1を返す。
 * @note       複数インターフェースがある機器では出力インターフェースを
 *             指定しないと意図しないインターフェースからマルチキャスト
 *             パケットが出てしまいます。 
 */
static int
enable_multicast(mc_info_t *info, char *errmsg)
{
  int rc = 0;
  in_addr_t ipaddr;

  /* IP_MULTICAST_IFを設定する */
  ipaddr = inet_addr("192.168.1.11"); /* 送信するLAN I/Fが持つIPアドレスを指定 */
  rc = setsockopt(info->sd, IPPROTO_IP, IP_MULTICAST_IF,
		  (char *)&ipaddr, sizeof(ipaddr));
  if(rc != 0){
    sprintf(errmsg, "(line:%d) %s", __LINE__, strerror(errno));
    return(-1);
  }

  /* マルチキャスト機能を有効にする */
  rc = setsockopt(info->sd, IPPROTO_IP, IP_MULTICAST_TTL,
		  (void *)&(info->ttl), sizeof(info->ttl));
  if(rc != 0){
    sprintf(errmsg, "(line:%d) %s", __LINE__, strerror(errno));
    return(-1);
  }

  return(0);
}

/*!
 * @brief      ソケットの初期化
 * @param[in]  info   マルチキャスト情報
 * @param[out] errmsg エラーメッセージ格納先
 * @return     成功ならば0、失敗ならば-1を返す。
 */
static int
socket_initialize(mc_info_t *info, char *errmsg)
{
  int rc = 0;

  /* ソケットの生成 : UDPを指定する */
  info->sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if(info->sd < 0){
    sprintf(errmsg, "(line:%d) %s", __LINE__, strerror(errno));
    return(-1);
  }

  /* マルチキャスト機能を有効にする */
  rc = enable_multicast(info, errmsg);
  if(rc != 0) return(-1);

  /* マルチキャストのアドレス構造体を作成する */
  info->addr.sin_family = AF_INET;
  info->addr.sin_addr.s_addr = inet_addr(info->ipaddr);
  info->addr.sin_port = htons(info->port);

  return(0);
}

/*!
 * @brief      ソケットの終期化
 * @param[in]  info   マルチキャスト情報
 * @return     成功ならば0、失敗ならば-1を返す。
 */
static void
socket_finalize(mc_info_t *info)
{
  /* ソケット破棄 */
  if(info->sd != 0) close(info->sd);

  return;
}

/*!
 * @brief      マルチキャストを送信する
 * @param[in]  info   マルチキャスト情報
 * @param[out] errmsg エラーメッセージ格納先
 * @return     成功ならば0、失敗ならば-1を返す。
 */
static int
multicast_sendmsg(mc_info_t *info, char *errmsg)
{
  int sendmsg_len = 0;

  /* マルチキャストを送信し続ける */
  while(1){
    sendmsg_len = sendto(info->sd, info->msg, info->msg_len, 0,
			 (struct sockaddr *)&(info->addr),
			 sizeof(info->addr));
    if(sendmsg_len != info->msg_len){
      sprintf(errmsg, "invalid msg is sent.(%s)",
	      strerror(errno));
      return(-1);
    }

    /* 5秒に一回送信する */
    sleep(5);
  }

  return(0);
}

/*!
 * @brief      マルチキャストを送信する
 * @param[in]  info   マルチキャスト情報
 * @param[out] errmsg エラーメッセージ格納先
 * @return     成功ならば0、失敗ならば-1を返す。
 */
static int
multicast_sender(mc_info_t *info, char *errmsg)
{
  int rc = 0;

  /* ソケットの初期化 */
  rc = socket_initialize(info, errmsg);
  if(rc != 0) return(-1);

  /* マルチキャストを送信する */
  rc = multicast_sendmsg(info, errmsg);

  /* ソケットの終期化 */
  socket_finalize(info);

  return(rc);
}

/*!
 * @brief      初期化処理。IPアドレスとポート番号を設定する。
 * @param[in]  argc   コマンドライン引数の数
 * @param[in]  argv   コマンドライン引数
 * @param[out] info   マルチキャスト情報
 * @param[out] errmsg エラーメッセージ格納先
 * @return     成功ならば0、失敗ならば-1を返す。
 */
static int
initialize(int argc, char *argv[], mc_info_t *info, char *errmsg)
{
  if(argc != 4){
    sprintf(errmsg, "usage: %s <multicast-addr> <port> <msg>", argv[0]);
    return(-1);
  }

  memset(info, 0, sizeof(mc_info_t));
  info->ipaddr  = argv[1];
  info->port    = atoi(argv[2]);
  info->msg     = argv[3];
  info->msg_len = strlen(argv[3]);
  info->ttl     = 1;  /* ホップ数を設定 */

  return(0);
}

/*!
 * @brief   main routine
 * @return  成功ならば0、失敗ならば-1を返す。
 */
int
main(int argc, char *argv[])
{
  int rc = 0;
  mc_info_t info = {0};
  char errmsg[BUFSIZ];

  rc = initialize(argc, argv, &info, errmsg);
  if(rc != 0){
    fprintf(stderr, "Error: %s\n", errmsg);
    return(-1);
  }

  rc = multicast_sender(&info, errmsg);
  if(rc != 0){
    fprintf(stderr, "Error: %s\n", errmsg);
    return(-1);
  }

  return(0);
}
