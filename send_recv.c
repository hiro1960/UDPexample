#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

/*
 *  ブロードキャスト情報（送信用）
 */
struct broadcast_info {
  unsigned short port;     /* ポート番号 */
  char *ipaddr;            /* IPアドレス */
  char *msg;               /* 送信メッセージ */
  unsigned int msg_len;    /* メッセージ長 */

  int sd;                  /* ソケットディスクリプタ */
  struct sockaddr_in addr; /* ブロードキャストのアドレス構造体 */
  int permission;          /* ブロードキャストの許可設定 */
};
typedef struct broadcast_info bc_info_t;

int count = 0;  /* 送信カウント */

/*
 *  ブロードキャスト情報（受信用）
 */
struct broadcast_info_r {
  unsigned short port;     /* ポート番号 */

  int sd;                  /* ソケットディスクリプタ */
  struct sockaddr_in addr; /* ブロードキャストのアドレス構造体 */
};
typedef struct broadcast_info_r bc_info_r;

#define MAXRECVSTRING 255  /* Longest string to receive */

/*!
 * @brief      ソケットの初期化
 * @param[in]  info   ブロードキャスト情報
 * @param[out] errmsg エラーメッセージ格納先
 * @return     成功ならば0、失敗ならば-1を返す。
 */
static int
socket_initialize(bc_info_t *info, char *errmsg)
{
  int rc = 0;

  /* ソケットの生成 : UDPを指定する */
  info->sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if(info->sd < 0){
    sprintf(errmsg, "(line:%d) %s", __LINE__, strerror(errno));
    return(-1);
  }

  /* ブロードキャスト機能を有効にする */
  rc = setsockopt(info->sd, SOL_SOCKET, SO_BROADCAST,
		  (void *)&(info->permission), sizeof(info->permission));
  if(rc != 0){
    sprintf(errmsg, "(line:%d) %s", __LINE__, strerror(errno));
    return(-1);
  }

  /* ブロードキャストのアドレス構造体を作成する */
  info->addr.sin_family = AF_INET;
  info->addr.sin_addr.s_addr = inet_addr(info->ipaddr);
  info->addr.sin_port = htons(info->port);

  return(0);
}

/*!
 * @brief      ソケットの初期化
 * @param[in]  info   ブロードキャスト情報
 * @param[out] errmsg エラーメッセージ格納先
 * @return     成功ならば0、失敗ならば-1を返す。
 */
static int
socket_initialize_r(bc_info_r *info_r, char *errmsg)
{
  int rc = 0;

  /* ソケットの生成 : UDPを指定する */
  info_r->sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if(info_r->sd < 0){
    sprintf(errmsg, "(line:%d) %s", __LINE__, strerror(errno));
    return(-1);
  }

  /* ブロードキャストのアドレス構造体を作成する */
  info_r->addr.sin_family = AF_INET;
  info_r->addr.sin_addr.s_addr = htonl(INADDR_ANY);
  info_r->addr.sin_port = htons(info_r->port);

  /* ブロードキャストポートにバインドする*/
  rc = bind(info_r->sd, (struct sockaddr *)&(info_r->addr),
      sizeof(info_r->addr));
  if(info_r->sd < 0){
    sprintf(errmsg, "(line:%d) %s", __LINE__, strerror(errno));
    return(-1);
  }

  return(0);
}

/*!
 * @brief      ソケットの終期化
 * @param[in]  info   ブロードキャスト情報
 * @return     成功ならば0、失敗ならば-1を返す。
 */
static void
socket_finalize(bc_info_t *info)
{
  /* ソケット破棄 */
  if(info->sd != 0) close(info->sd);

  return;
}

/*!
 * @brief      ブロードキャストを送信する
 * @param[in]  info   ブロードキャスト情報
 * @param[out] errmsg エラーメッセージ格納先
 * @return     成功ならば0、失敗ならば-1を返す。
 */
static int
broadcast_sendmsg(bc_info_t *info, bc_info_r *info_r, char *errmsg)
{
  int sendmsg_len = 0;
  char line[256];
  char *lmsg = info->msg;

  char recv_msg[MAXRECVSTRING+1];
  int recv_msglen = 0;
  
  /* ブロードキャストを送信し続ける */
  while(1){
    sprintf(line, "%s %d", lmsg, count++);
    info->msg = line;
    info->msg_len = strlen(line);
    
    sendmsg_len = sendto(info->sd, info->msg, info->msg_len, 0,
			 (struct sockaddr *)&(info->addr),
			 sizeof(info->addr));
    if(sendmsg_len != info->msg_len){
      sprintf(errmsg, "invalid msg is sent.(%s)",
	      strerror(errno));
      return(-1);
    }

    printf("Sended\n" );

    /* すぐに受信する */
    recv_msglen = recvfrom(info_r->sd, recv_msg, MAXRECVSTRING, 0, NULL, 0);
    if(recv_msglen < 0){
      sprintf(errmsg, "(line:%d) %s", __LINE__, strerror(errno));
      return(-1);
    }

    recv_msg[recv_msglen] = '\0';
    printf("Received: %s\n", recv_msg);    /* Print the received string */

    /* 5秒に一回送信する */
    sleep(5);
  }

  return(0);
}

/*!
 * @brief      ブロードキャストを送信する
 * @param[in]  info   ブロードキャスト情報
 * @param[out] errmsg エラーメッセージ格納先
 * @return     成功ならば0、失敗ならば-1を返す。
 */
static int
broadcast_sender(bc_info_t *info, bc_info_r *info_r, char *errmsg)
{
  int rc = 0;

  /* ソケットの初期化（送信用） */
  rc = socket_initialize(info, errmsg);
  if(rc != 0) return(-1);

  /* ソケットの初期化（受信用） */
  rc = socket_initialize_r(info_r, errmsg);
  if(rc != 0) return(-1);

  /* ブロードキャストを送信する */
  rc = broadcast_sendmsg(info, info_r, errmsg);
  /* 注意：sendmsg()内では５秒毎に送信の無限loop 戻ってこない */

  
  /* ソケットの終期化 */
  socket_finalize(info);

  return(0);
}

/*!
 * @brief      初期化処理。IPアドレスとポート番号を設定する。
 * @param[in]  argc   コマンドライン引数の数
 * @param[in]  argv   コマンドライン引数
 * @param[out] info   ブロードキャスト情報（送信用）
 * @param[out] info_r  ブロードキャスト情報（受信用）
 * @param[out] errmsg エラーメッセージ格納先
 * @return     成功ならば0、失敗ならば-1を返す。
 */
static int
initialize(int argc, char *argv[], bc_info_t *info, bc_info_r *info_r, char *errmsg)
{
  if(argc != 4){
    sprintf(errmsg, "Usage: %s <ip-addr> <port> <msg>", argv[0]);
    return(-1);
  }

  /* 送信用の初期化設定 */
  memset(info, 0, sizeof(bc_info_t));
  info->ipaddr     = argv[1];
  info->port       = atoi(argv[2]);
  info->msg        = argv[3];
  info->msg_len    = strlen(argv[3]);
  info->permission = 1;

  /* 受信用の初期化設定 */
  memset(info_r, 0, sizeof(bc_info_r));
  info_r->port       = atoi(argv[2]);

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
  bc_info_t info = {0};
  bc_info_r info_r = {0};
  char errmsg[BUFSIZ];

  rc = initialize(argc, argv, &info, &info_r, errmsg);
  if(rc != 0){
    fprintf(stderr, "Error: %s\n", errmsg);
    return(-1);
  }

  rc = broadcast_sender(&info, &info_r, errmsg);
  if(rc != 0){
    fprintf(stderr, "Error: %s\n", errmsg);
    return(-1);
  }

  return(0);
}
