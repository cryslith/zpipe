#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <com_err.h>
#include <zephyr/zephyr.h>

#include "zpipe.h"


int main(void) {
  int status;

  if ((status = ZInitialize()) != ZERR_NONE) {
    com_err("zpipe", status, "while initializing");
    return 1;
  }

  int ret;
  ssize_t n;
  char *sender;
  char *klass;
  char *instance;
  char *recipient;
  char *opcode;
  char *auth;
  char *message_length;
  char *message;

  for (int cont = 1; cont;) {
    ret = 0;
    cont = 0;

    ssize_t s;
    n = 0;
    if ((s = getdelim(&sender, &s, 0, stdin)) == -1) {
      continue;
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
        goto free_all;
      }
    }
    else {
      message = "";
    }

    if ((status = zwrite(sender, klass, instance,
                         recipient, opcode, mlen, message,
                         atoi(auth))) != ZERR_NONE) {
      com_err("zpipe", status, "during zwrite");
      ret = 1;
      cont = 0;
      goto free_all;
    }

  free_all:
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
  }

  return ret;
}

int zwrite(char *sender, char *klass, char *instance,
           char *recipient, char *opcode,
           int message_len, char *message, int auth) {
  ZNotice_t notice;
  memset((void *) &notice, 0, sizeof(notice));

  notice.z_kind = ACKED;
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
