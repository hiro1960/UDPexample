#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

/*
 *  ブロードキャスト情報
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
broadcast_sendmsg(bc_info_t *info, char *errmsg)
{
  int sendmsg_len = 0;
  char line[256];
  char *lmsg = info->msg;
  
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
broadcast_sender(bc_info_t *info, char *errmsg)
{
  int rc = 0;

  /* ソケットの初期化 */
  rc = socket_initialize(info, errmsg);
  if(rc != 0) return(-1);

  /* ブロードキャストを送信する */
  rc = broadcast_sendmsg(info, errmsg);
  /* 注意：sendmsg()内では５秒毎に送信の無限loop 戻ってこない */

  
  /* ソケットの終期化 */
  socket_finalize(info);

  return(0);
}

/*!
 * @brief      初期化処理。IPアドレスとポート番号を設定する。
 * @param[in]  argc   コマンドライン引数の数
 * @param[in]  argv   コマンドライン引数
 * @param[out] info   ブロードキャスト情報
 * @param[out] errmsg エラーメッセージ格納先
 * @return     成功ならば0、失敗ならば-1を返す。
 */
static int
initialize(int argc, char *argv[], bc_info_t *info, char *errmsg)
{
  if(argc != 4){
    sprintf(errmsg, "Usage: %s <ip-addr> <port> <msg>", argv[0]);
    return(-1);
  }

  memset(info, 0, sizeof(bc_info_t));
  info->ipaddr     = argv[1];
  info->port       = atoi(argv[2]);
  info->msg        = argv[3];
  info->msg_len    = strlen(argv[3]);
  info->permission = 1;

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
  char errmsg[BUFSIZ];

  rc = initialize(argc, argv, &info, errmsg);
  if(rc != 0){
    fprintf(stderr, "Error: %s\n", errmsg);
    return(-1);
  }

  rc = broadcast_sender(&info, errmsg);
  if(rc != 0){
    fprintf(stderr, "Error: %s\n", errmsg);
    return(-1);
  }

  return(0);
}
