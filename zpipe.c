#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <com_err.h>
#include <zephyr/zephyr.h>

#include "zpipe.h"

#define CHECK_ZERR(X) do {                        \
    Code_t status = X;                            \
    if (status != ZERR_NONE) {                    \
      com_err("zpipe", status, #X);               \
      return 1;                                   \
    }                                             \
  } while (0)


int main(void) {
  CHECK_ZERR(ZInitialize());

  ZSubscription_t ex_subs[1];
  ex_subs[0].zsub_class = "ikdc-test";
  ex_subs[0].zsub_classinst = "*";
  ex_subs[0].zsub_recipient = "*";
  CHECK_ZERR(ZSubscribeToSansDefaults(ex_subs, 1, 0));
  ex_subs[0].zsub_class = "message";
  ex_subs[0].zsub_classinst = "zpipe";
  ex_subs[0].zsub_recipient = "ikdc@ATHENA.MIT.EDU";
  CHECK_ZERR(ZSubscribeToSansDefaults(ex_subs, 1, 0));

  int nsubs;
  ZSubscription_t *subs;

  CHECK_ZERR(ZRetrieveSubscriptions(0, &nsubs));
  subs = malloc(sizeof(ZSubscription_t) * nsubs);
  CHECK_ZERR(ZGetSubscriptions(subs, &nsubs));
  for (int i = 0; i < nsubs; i++) {
    printf("(%s, %s, %s)\n", subs[i].zsub_class, subs[i].zsub_classinst, subs[i].zsub_recipient);
  }
  free(subs);

  // TODO branch
  return notice_loop();
  return stdin_loop();
}

int stdin_loop(void) {
  int cont;
  char *command;
  ssize_t n;

  for (;;) {
    command = NULL;
    n = 0;
    if (getdelim(&command, &n, 0, stdin) < 0) {
      return 0;
    }
    if (strcmp(command, "zwrite") == 0) {
      free(command);
      CHECK_ZERR(receive_zwrite(&cont));
      if (cont) {
        continue;
      }
      break;
    }
    // TODO subscribe and unsubscribe
    free(command);
    fprintf(stderr, "unknown command\n");
    return 1;
  }
}

Code_t receive_zwrite(int *cont) {
  ssize_t n;
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

  ssize_t s;
  n = 0;
  if ((s = getdelim(&sender, &n, 0, stdin)) == -1) {
    goto done_freeing;
  }
  if (s <= 1) {
    free(sender);
    sender = ZGetSender();
  }
  n = 0;
  if (getdelim(&klass, &n, 0, stdin) == -1) {
    goto free_sender;
  }
  n = 0;
  if (getdelim(&instance, &n, 0, stdin) == -1) {
    goto free_klass;
  }
  n = 0;
  if (getdelim(&recipient, &n, 0, stdin) == -1) {
    goto free_instance;
  }
  n = 0;
  if (getdelim(&opcode, &n, 0, stdin) == -1) {
    goto free_recipient;
  }
  n = 0;
  if (getdelim(&auth, &n, 0, stdin) == -1) {
    goto free_opcode;
  }
  n = 0;
  if (getdelim(&message_length, &n, 0, stdin) == -1) {
    goto free_auth;
  }
  int mlen = atoi(message_length);
  if (mlen) {
    if (!(message = malloc(mlen))) {
      goto free_message_length;
    }
    if (!(fgets(message, mlen + 1, stdin))) {
      goto free_message;
    }
  }
  else {
    message = "";
  }

  if ((err = zwrite(sender, klass, instance,
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
  if (s) {
    free(sender);
  }

 done_freeing:
  return err;
}

Code_t zwrite(char *sender, char *klass, char *instance,
              char *recipient, char *opcode,
              int message_len, char *message, int auth) {
  ZNotice_t notice;
  memset((void *) &notice, 0, sizeof(notice));

  notice.z_kind = UNACKED;
  notice.z_port = 0;
  notice.z_default_format = "http://mit.edu/df";
  notice.z_charset = ZCHARSET_UTF_8;

  notice.z_sender = sender;
  notice.z_class = klass;
  notice.z_class_inst = instance;
  notice.z_recipient = recipient;
  notice.z_opcode = opcode;
  notice.z_message_len = message_len;
  notice.z_message = message;

  return ZSendNotice(&notice, auth ? ZAUTH : ZNOAUTH);
}

int notice_loop(void) {
  for (;;) {
    CHECK_ZERR(receive_notice());
  }
}

Code_t receive_notice(void) {
  printf("waiting...\n");
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
    if (authed == ZAUTH_NO) {
      printf("auth: no\n");
    }
    else if (authed == ZAUTH_YES) {
      printf("auth: yes\n");
    }
    else {
      printf("auth: failed\n");
    }

    printf("time: %u:%u\n", notice->z_time.tv_sec, notice->z_time.tv_usec);
    printf("sender: %s\n", notice->z_sender);
    printf("class: %s\n", notice->z_class);
    printf("instance: %s\n", notice->z_class_inst);
    printf("recipient: %s\n", notice->z_recipient);
    printf("opcode: %s\n", notice->z_opcode);
    printf("message len: %u\n", notice->z_message_len);
    printf("message: %s\n", notice->z_message);
  }
  ZFreeNotice(notice);

  return ZERR_NONE;
}
