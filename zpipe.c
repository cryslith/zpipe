#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <error.h>
#include <errno.h>
#include <sys/select.h>

#include <com_err.h>
#include <zephyr/zephyr.h>

#include "zpipe.h"
#include "util.h"
#include "list.h"


#define ASSERT_ZERR(X) do {                           \
    Code_t status = X;                                \
    if (status != ZERR_NONE) {                        \
      com_err("zpipe", status, "calling %s at %s:%u", \
              #X, __FILE__, __LINE__);                \
      exit(1);                                        \
    }                                                 \
  } while (0)

#define PARSE_ARG(ARG, KEY, CONTINUE) do {            \
      if (strcmp((ARG)->arg_key, #KEY) == 0) {        \
        if (KEY != NULL) {                            \
          ERROR("duplicate key: " #KEY "\n");         \
        }                                             \
        KEY = (ARG)->arg_value;                       \
        goto CONTINUE;                                \
      }                                               \
  } while (0)                                         \


Code_t write_zerr(Code_t err, char *operation) {
  if (err != ZERR_NONE) {
    send_err(operation, error_message(err));
  }
  return err;
}


typedef struct _arg {
  char *arg_key;
  char *arg_value;
} _arg, *arg;

void arg_list_free(list args) {
  arg a;
  for (list l = args; l != nil; l = tail(l)) {
    a = head(l);
    free(a->arg_key);
    free(a->arg_value);
    free(a);
  }
  list_free(args);
}


/* Returns a list arg, all of whose substructures must be free()d.
 */
list collect_arguments() {
  ssize_t r, n;
  list tmp, acc = nil;
  char *key, *value;
  arg a;
  for (;;) {
    n = 0;
    getdelim_unbuf(&key, &n, 0, STDIN_FILENO);
    if (strlen(key) == 0) {
      return acc;
    }
    n = 0;
    if ((r = getdelim_unbuf(&value, &n, 0, STDIN_FILENO)) < 0) {
      ERROR("eof reading value");
    }
    if ((a = malloc(sizeof(_arg))) == NULL) {
      ERROR("malloc returned null");
    }
    a->arg_key = key;
    a->arg_value = value;
    if ((tmp = cons(a, acc)) == nil) {
      ERROR("cons returned nil");
    }
    acc = tmp;
  }
}

void debug_arguments(list args) {
  arg a;
  for (list l = args; l != nil; l = tail(l)) {
    a = head(l);
    fprintf(stderr, "%s: %s\n", a->arg_key, a->arg_value);
  }
}

int main(void) {
  int r;

  ASSERT_ZERR(ZInitialize());

  int subscribed = 0;

  write_zerr(ZSubscribeToSansDefaults(NULL, 0, 0), "initializing");

  int zfd = ZGetFD();
  int infd = STDIN_FILENO;
  int maxfd;
  fd_set readfds;
  for (;;) {
    FD_ZERO(&readfds);
    maxfd = -1;
    if (zfd >= 0) {
      FD_SET(zfd, &readfds);
      maxfd = max(maxfd, zfd);
    }
    if (infd >= 0) {
      FD_SET(infd, &readfds);
      maxfd = max(maxfd, infd);
    }
    if (maxfd < 0) {
      return 0;
    }
    if ((r = select(maxfd + 1, &readfds, NULL, NULL, NULL)) < 0) {
      error_at_line(1, errno, __FILE__, __LINE__, "error on select");
    }
    if (FD_ISSET(infd, &readfds)) {
      int close_zephyr;
      if (!receive_stdin(&close_zephyr)) {
        close(infd);
        infd = -1;
      }
      if (close_zephyr) {
        write_zerr(ZCancelSubscriptions(0), "closing zephyr");
        zfd = -1;
        fclose(stdout);
      }
    }
    if (FD_ISSET(zfd, &readfds)) {
      write_zerr(receive_notice(), "receiving notice");
    }
  }
}

// returns 0 on eof
int receive_stdin(int *close_zephyr) {
  *close_zephyr = 0;

  list args = collect_arguments();

  char *command = NULL;
  for (list l = args; l != nil; l = tail(l)) {
    arg a = head(l);
    PARSE_ARG(a, command, stdin_args_cont);

    ERROR("unknown key %s\n", a->arg_key);
  stdin_args_cont:;
  }

  int cont = 1;
  if (command == NULL) {
    cont = 0;
    goto stdin_done;
  }
  if (strcmp(command, "zwrite") == 0) {
    receive_zwrite();
    goto stdin_done;
  }
  if (strcmp(command, "subscribe") == 0) {
    receive_subscription(1);
    goto stdin_done;
  }
  if (strcmp(command, "unsubscribe") == 0) {
    receive_subscription(0);
    goto stdin_done;
  }
  if (strcmp(command, "close_zephyr") == 0) {
    *close_zephyr = 1;
    goto stdin_done;
  }
  ERROR("unknown command\n");

 stdin_done:
  arg_list_free(args);
  return cont;
}

void receive_zwrite(void) {
  list args = collect_arguments();

  char *charset = NULL;
  char *sender = NULL;
  char *class = NULL;
  char *instance = NULL;
  char *recipient = NULL;
  char *opcode = NULL;
  char *auth = NULL;
  char *message_length = NULL;
  for (list l = args; l != nil; l = tail(l)) {
    arg a = head(l);
    PARSE_ARG(a, charset, zwrite_args_cont);
    PARSE_ARG(a, sender, zwrite_args_cont);
    PARSE_ARG(a, class, zwrite_args_cont);
    PARSE_ARG(a, instance, zwrite_args_cont);
    PARSE_ARG(a, recipient, zwrite_args_cont);
    PARSE_ARG(a, opcode, zwrite_args_cont);
    PARSE_ARG(a, auth, zwrite_args_cont);
    PARSE_ARG(a, message_length, zwrite_args_cont);

    ERROR("unknown zwrite key %s\n", a->arg_key);
  zwrite_args_cont:;
  }

  if (!charset) {
    charset = "UTF-8";
  }
  if (!sender) {
    sender = ZGetSender();
  }
  if (!class) {
    class = "MESSAGE";
  }
  if (!instance) {
    instance = "personal";
  }
  if (!recipient) {
    recipient = "";
  }
  if (!opcode) {
    opcode = "";
  }
  if (!auth) {
    auth = "1";
  }
  if (!message_length) {
    message_length = "0";
  }

  char *message = NULL;
  int mlen = atoi(message_length);
  if (mlen) {
    if (!(message = malloc(mlen))) {
      ERROR("error malloc()ing for message\n");
    }
    if (read_all(STDIN_FILENO, message, mlen) < mlen) {
      ERROR("unexpected EOF reading message\n");
    }
  }

  write_zerr(zwrite(ZGetCharset(charset),
                    sender, class, instance, recipient, opcode,
                    mlen, message, atoi(auth)),
             "zwriting");

  if (message) {
    free(message);
  }
  arg_list_free(args);
}

Code_t zwrite(unsigned short charset,
              char *sender, char *class, char *instance,
              char *recipient, char *opcode,
              int message_len, char *message, int auth) {
  ZNotice_t notice;
  memset((void *) &notice, 0, sizeof(notice));

  notice.z_kind = UNACKED;
  notice.z_port = 0;
  notice.z_default_format = "http://mit.edu/df";
  notice.z_charset = charset;

  notice.z_sender = sender;
  notice.z_class = class;
  notice.z_class_inst = instance;
  notice.z_recipient = recipient;
  notice.z_opcode = opcode;
  notice.z_message_len = message_len;
  notice.z_message = message;

  return ZSendNotice(&notice, auth ? ZAUTH : ZNOAUTH);
}

void receive_subscription(int should_sub) {
  list args = collect_arguments();

  char *class = NULL;
  char *instance = NULL;
  char *recipient = NULL;
  for (list l = args; l != nil; l = tail(l)) {
    arg a = head(l);
    PARSE_ARG(a, class, sub_args_cont);
    PARSE_ARG(a, instance, sub_args_cont);
    PARSE_ARG(a, recipient, sub_args_cont);

    ERROR("unknown subscription key %s\n", a->arg_key);
  sub_args_cont:;
  }

  if (!class) {
    ERROR("no class specified\n");
  }
  if (!instance) {
    instance = "*";
  }
  if (!recipient) {
    recipient = "*";
  }

  int success = 0;
  if (write_zerr(subscribe(class, instance, recipient, should_sub,
                           &success),
                 "changing subscriptions") == ZERR_NONE) {
    if (!success) {
      send_err("changing subscriptions", "subscriptions did not change");
    }
  }

  arg_list_free(args);
}

Code_t subscribe(char *class, char *instance, char *recipient, int should_sub,
                 int *success) {
  ZSubscription_t sub;
  Code_t r;
  sub.zsub_class = class;
  sub.zsub_classinst = instance;
  sub.zsub_recipient = recipient;
  if ((r = (should_sub ? ZSubscribeToSansDefaults(&sub, 1, 0) :
            ZUnsubscribeTo(&sub, 1, 0))) != ZERR_NONE) {
    return r;
  }
  int have_sub = 1;
  if ((r = ensure_subscription(class, instance, recipient, &have_sub)) !=
      ZERR_NONE) {
    return r;
  }
  *success = (!have_sub) == (!should_sub);
  return ZERR_NONE;
}

void debug_subscriptions() {
  int n;
  ASSERT_ZERR(ZRetrieveSubscriptions(0, &n));

  ZSubscription_t sub;
  int one = 1;
  fprintf(stderr, "%d subscriptions:\n", n);
  for (int i = 0; i < n; i++) {
    one = 1;
    ASSERT_ZERR(ZGetSubscriptions(&sub, &one));
    fprintf(stderr, "(%s, %s, %s)\n",
            sub.zsub_class, sub.zsub_classinst, sub.zsub_recipient);
  }
}

Code_t ensure_subscription(char *class, char *instance, char *recipient,
                           int *should_have) {
  int n;
  Code_t r;
  if ((r = ZRetrieveSubscriptions(0, &n)) != ZERR_NONE) {
    return r;
  }
  ZSubscription_t sub;
  int one = 1;
  int has = 0;
  for (int i = 0; i < n; i++) {
    if ((r = ZGetSubscriptions(&sub, &one)) != ZERR_NONE) {
      return r;
    }
    if (one != 1) {
      return ZERR_NOMORESUBSCRIPTIONS;
    }
    if (strcmp(sub.zsub_class, class) == 0 &&
        strcmp(sub.zsub_classinst, instance) == 0 &&
        strcmp(sub.zsub_recipient, recipient) == 0) {
      *should_have = 1;
      return ZERR_NONE;
    }
  }
  *should_have = 0;
  return ZERR_NONE;
}

Code_t receive_notice(void) {
  ZNotice_t notice;
  struct sockaddr_in from;
  Code_t status;
  if ((status = ZReceiveNotice(&notice, &from)) != ZERR_NONE) {
    return status;
  }
  process_notice(&notice, &from);
  return ZERR_NONE;
}

void process_notice(ZNotice_t *notice, struct sockaddr_in *from) {
  if (notice->z_kind == UNSAFE ||
      notice->z_kind == UNACKED ||
      notice->z_kind == ACKED) {
    Code_t authed = ZCheckAuthentication(notice, from);

#define PROCESS_NOTICE_KEYS(PRINTF) do {                                \
    PRINTF("charset%c%s%c", 0, ZCharsetToString(notice->z_charset), 0); \
    PRINTF("timestamp%c%u:%u%c", 0,                                     \
           notice->z_time.tv_sec, notice->z_time.tv_usec, 0);           \
    PRINTF("sender%c%s%c", 0, notice->z_sender, 0);                     \
    PRINTF("class%c%s%c", 0, notice->z_class, 0);                       \
    PRINTF("instance%c%s%c", 0, notice->z_class_inst, 0);               \
    PRINTF("recipient%c%s%c", 0, notice->z_recipient, 0);               \
    PRINTF("opcode%c%s%c", 0, notice->z_opcode, 0);                     \
    PRINTF("auth%c%u%c", 0, authed == ZAUTH_YES, 0);                    \
    PRINTF("message_length%c%u%c", 0, notice->z_message_len, 0);        \
    PRINTF("%c", 0);                                                    \
  } while (0)

    int len = 0;
#define P_N_ADD_LEN(...) len += snprintf(NULL, 0, __VA_ARGS__)

    PROCESS_NOTICE_KEYS(P_N_ADD_LEN);
    len += notice->z_message_len;

    printf("type%cnotice%c", 0, 0);
    printf("length%c%d%c", 0, len, 0);
    printf("%c", 0);

    PROCESS_NOTICE_KEYS(printf);
    fwrite(notice->z_message, 1, notice->z_message_len, stdout);
    fflush(stdout);
  }
  ZFreeNotice(notice);
}

void send_err(char *operation, const char *err) {
#define SEND_ERR_KEYS(PRINTF) do {                                      \
    if (operation) {                                                    \
      PRINTF("operation%c%s%c", 0, operation, 0);                       \
    }                                                                   \
    PRINTF("message%c%s%c", 0, err, 0);                                 \
    PRINTF("%c", 0);                                                    \
  } while (0)

  int len = 0;
#define S_E_ADD_LEN(...) len += snprintf(NULL, 0, __VA_ARGS__)

  SEND_ERR_KEYS(S_E_ADD_LEN);

  printf("type%cerror%c", 0, 0);
  printf("length%c%d%c", 0, len, 0);
  printf("%c", 0);

  SEND_ERR_KEYS(printf);
  fflush(stdout);
}
