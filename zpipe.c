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

#define CHECK_ZERR(X) do {                        \
    Code_t status = X;                            \
    if (status != ZERR_NONE) {                    \
      com_err("zpipe", status, #X);               \
      return 1;                                   \
    }                                             \
  } while (0)


int main(void) {
  int r;

  CHECK_ZERR(ZInitialize());

  CHECK_ZERR(ZSubscribeToSansDefaults(NULL, 0, 0));

  int zfd = ZGetFD();
  int infd = STDIN_FILENO;
  int maxfd;
  fd_set readfds;
  for (int i = 0;;i++) {
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
      error(1, errno, "select");
    }
    if (FD_ISSET(infd, &readfds)) {
      int cont, close_zephyr;
      if ((r = receive_stdin(&cont, &close_zephyr)) != 0) {
        return r;
      }
      if (!cont) {
        close(infd);
        infd = -1;
      }
      if (close_zephyr) {
        CHECK_ZERR(ZCancelSubscriptions(0));
        zfd = -1;
        fclose(stdout);
      }
    }
    if (FD_ISSET(zfd, &readfds)) {
      CHECK_ZERR(receive_notice());
    }
  }
}

int receive_stdin(int *cont, int *close_zephyr) {
  char *command = NULL;
  *cont = 0;
  *close_zephyr = 0;

  ssize_t n = 0;
  ssize_t r;
  if (getdelim_unbuf(&command, &n, 0, STDIN_FILENO) < 0) {
    fprintf(stderr, "error reading command\n");
    return 1;
  }
  if (strlen(command) == 0) {
    // empty command
    return 0;
  }

  if (strcmp(command, "zwrite") == 0) {
    free(command);
    CHECK_ZERR(receive_zwrite(cont));
    return 0;
  }
  if (strcmp(command, "subscribe") == 0) {
    free(command);
    CHECK_ZERR(receive_subscription(cont, 0));
    return 0;
  }
  if (strcmp(command, "unsubscribe") == 0) {
    free(command);
    CHECK_ZERR(receive_subscription(cont, 1));
    return 0;
  }
  if (strcmp(command, "close_zephyr") == 0) {
    free(command);
    *close_zephyr = 1;
    *cont = 1;
    return 0;
  }

  free(command);
  fprintf(stderr, "unknown command\n");
  return 1;
}

Code_t receive_zwrite(int *cont) {
  ssize_t n;
  char *charset = NULL;
  char *sender = NULL;
  char *klass = NULL;
  char *instance = NULL;
  char *recipient = NULL;
  char *opcode = NULL;
  char *auth = NULL;
  char *message_length = NULL;
  char *message;

  Code_t err = ZERR_NONE;
  *cont = 0;

  n = 0;
  if (getdelim_unbuf(&charset, &n, 0, STDIN_FILENO) == -1) {
    goto done_freeing;
  }
  ssize_t s;
  n = 0;
  if ((s = getdelim_unbuf(&sender, &n, 0, STDIN_FILENO)) == -1) {
    goto free_charset;
  }
  if (s <= 1) {
    free(sender);
    sender = ZGetSender();
  }
  n = 0;
  if (getdelim_unbuf(&klass, &n, 0, STDIN_FILENO) == -1) {
    goto free_sender;
  }
  n = 0;
  if (getdelim_unbuf(&instance, &n, 0, STDIN_FILENO) == -1) {
    goto free_klass;
  }
  n = 0;
  if (getdelim_unbuf(&recipient, &n, 0, STDIN_FILENO) == -1) {
    goto free_instance;
  }
  n = 0;
  if (getdelim_unbuf(&opcode, &n, 0, STDIN_FILENO) == -1) {
    goto free_recipient;
  }
  n = 0;
  if (getdelim_unbuf(&auth, &n, 0, STDIN_FILENO) == -1) {
    goto free_opcode;
  }
  n = 0;
  if (getdelim_unbuf(&message_length, &n, 0, STDIN_FILENO) == -1) {
    goto free_auth;
  }
  int mlen = atoi(message_length);
  if (mlen) {
    if (!(message = malloc(mlen))) {
      goto free_message_length;
    }
    if (read_all(STDIN_FILENO, message, mlen) < mlen) {
      goto free_message;
    }
  }
  else {
    message = "";
  }

  if ((err = zwrite(ZGetCharset(charset),
                    sender, klass, instance,
                    recipient, opcode, mlen, message,
                    atoi(auth))) == ZERR_NONE) {
    *cont = 1;
  }

 free_message:
  if (mlen) {
    free(message);
  }
 free_message_length:
  free(message_length);
 free_auth:
  free(auth);
 free_opcode:
  free(opcode);
 free_recipient:
  free(recipient);
 free_instance:
  free(instance);
 free_klass:
  free(klass);
 free_sender:
  if (s > 1) {
    free(sender);
  }
 free_charset:
  free(charset);

 done_freeing:
  return err;
}

Code_t zwrite(unsigned short charset,
              char *sender, char *klass, char *instance,
              char *recipient, char *opcode,
              int message_len, char *message, int auth) {
  ZNotice_t notice;
  memset((void *) &notice, 0, sizeof(notice));

  notice.z_kind = UNACKED;
  notice.z_port = 0;
  notice.z_default_format = "http://mit.edu/df";
  notice.z_charset = charset;

  notice.z_sender = sender;
  notice.z_class = klass;
  notice.z_class_inst = instance;
  notice.z_recipient = recipient;
  notice.z_opcode = opcode;
  notice.z_message_len = message_len;
  notice.z_message = message;

  return ZSendNotice(&notice, auth ? ZAUTH : ZNOAUTH);
}

Code_t receive_subscription(int *cont, int unsub) {
  ssize_t n;
  char *klass = NULL;
  char *instance = NULL;
  char *recipient = NULL;

  Code_t err = ZERR_NONE;
  *cont = 0;

  n = 0;
  if (getdelim_unbuf(&klass, &n, 0, STDIN_FILENO) == -1) {
    goto done_freeing;
  }
  n = 0;
  if (getdelim_unbuf(&instance, &n, 0, STDIN_FILENO) == -1) {
    goto free_klass;
  }
  n = 0;
  if (getdelim_unbuf(&recipient, &n, 0, STDIN_FILENO) == -1) {
    goto free_instance;
  }

  if (unsub) {
    err = unsubscribe(klass, instance, recipient);
  }
  else {
    err = subscribe(klass, instance, recipient);
  }
  if (err == ZERR_NONE) {
    *cont = 1;
  }

  free(recipient);
 free_instance:
  free(instance);
 free_klass:
  free(klass);

 done_freeing:
  return err;
}

Code_t subscribe(char *klass, char *instance, char *recipient) {
  ZSubscription_t sub;
  sub.zsub_class = klass;
  sub.zsub_classinst = instance;
  sub.zsub_recipient = recipient;
  return ZSubscribeToSansDefaults(&sub, 1, 0);
}

Code_t unsubscribe(char *klass, char *instance, char *recipient) {
  ZSubscription_t sub;
  sub.zsub_class = klass;
  sub.zsub_classinst = instance;
  sub.zsub_recipient = recipient;
  return ZUnsubscribeTo(&sub, 1, 0);
}

Code_t receive_notice(void) {
  ZNotice_t notice;
  struct sockaddr_in from;
  Code_t status;
  if ((status = ZReceiveNotice(&notice, &from)) != ZERR_NONE) {
    return status;
  }
  return process_notice(&notice, &from);
}

Code_t process_notice(ZNotice_t *notice, struct sockaddr_in *from) {
  if (notice->z_kind == UNSAFE ||
      notice->z_kind == UNACKED ||
      notice->z_kind == ACKED) {
    Code_t authed = ZCheckAuthentication(notice, from);

    fprintf(stdout,
            "notice%c%s%c%u:%u%c%s%c%s%c%s%c%s%c%s%c%u%c%u%c", 0,
            ZCharsetToString(notice->z_charset), 0,
            notice->z_time.tv_sec, notice->z_time.tv_usec, 0,
            notice->z_sender, 0,
            notice->z_class, 0,
            notice->z_class_inst, 0,
            notice->z_recipient, 0,
            notice->z_opcode, 0,
            authed == ZAUTH_YES, 0,
            notice->z_message_len, 0);
    fwrite(notice->z_message, 1, notice->z_message_len, stdout);
    fflush(stdout);
  }
  ZFreeNotice(notice);

  return ZERR_NONE;
}
