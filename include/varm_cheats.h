#ifndef VARM_CHEATS_H
#define VARM_CHEATS_H

void varm_cheats_init(void);
void varm_cheats_inject(void);
int varm_cheats_parse_line(const char *line); // <--- Add this line right here!

#endif // VARM_CHEATS_H
