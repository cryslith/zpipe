#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <com_err.h>
#include <zephyr/zephyr.h>


int main(void) {
  int status;

  if ((status = ZInitialize()) != ZERR_NONE) {
    com_err("zpipe", status, "while initializing");
    return 1;
  }

  if ((status = zwrite(ZGetSender(), "MESSAGE", "zpipe",
                       ZGetSender(), "zpipe", "zpiping",
                       "hi there", 1)) != ZERR_NONE) {
    com_err("zpipe", status, "during zwrite");
    return 1;
  }

  return 0;
}

int zwrite(char *sender, char *klass, char *instance,
           char *recipient, char *opcode,
           char *zsig, char *message, int auth) {
  ZNotice_t notice;
  int zsig_len, message_len;
  char *body;
  int (*cert)();

  memset((void *) &notice, 0, sizeof(notice));

  zsig_len = strlen(zsig) + 1;
  message_len = strlen(message) + 1;
  body = malloc(zsig_len + message_len);
  strcpy(body, zsig);
  strcpy(body + zsig_len, message);

  notice.z_kind = ACKED;
  notice.z_port = 0;

  notice.z_sender = sender;
  notice.z_class = klass;
  notice.z_class_inst = instance;
  notice.z_recipient = recipient;
  notice.z_opcode = opcode;
  notice.z_default_format = "http://mit.edu/df";
  notice.z_charset = ZCHARSET_UTF_8;
  notice.z_message = body;
  notice.z_message_len = zsig_len + message_len;

  cert = auth ? ZAUTH : ZNOAUTH;

  return ZSendNotice(&notice, cert);
}
