//
// Created by peleg on 17/05/2025.
//

#ifndef LIBTOURNAMENT_H
#define LIBTOURNAMENT_H
// user/libtournament.c
int tournament_create(int processes);
int tournament_acquire(void);
int tournament_release(void);
#endif //LIBTOURNAMENT_H

