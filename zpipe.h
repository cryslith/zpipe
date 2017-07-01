int stdin_loop(void);
Code_t receive_zwrite(int *err);
Code_t zwrite(char *sender, char *klass, char *instance,
              char *recipient, char *opcode,
              int message_len, char *message, int auth);

int notice_loop(void);
Code_t receive_notice(void);
Code_t process_notice(ZNotice_t *notice, struct sockaddr_in *from);
