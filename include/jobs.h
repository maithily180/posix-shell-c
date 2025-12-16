#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>
#include <stdbool.h>

#define MAX_JOBS 16

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_COMPLETED
} JobState;

typedef struct {
    int job_number;
    pid_t pid;
    char *command;
    JobState state;
    bool is_background;
} Job;

// Job management functions
void jobs_init(void);
int jobs_add(pid_t pid, const char *command, bool is_background);
void jobs_remove(int job_number);
Job *jobs_get(int job_number);
Job *jobs_get_by_pid(pid_t pid);
void jobs_check_completed(void);
void jobs_print_job(int job_number, pid_t pid);
int jobs_get_next_number(void);
void jobs_cleanup(void);
void jobs_kill_all(void);

// Part E functions
void jobs_list_activities(void);
int jobs_send_signal(int job_number, int signal_num);
int jobs_bring_to_foreground(int job_number);
int jobs_resume_background(int job_number);
int jobs_get_most_recent_job(void);
void jobs_set_stopped(int job_number);
void jobs_set_running(int job_number);

#endif // JOBS_H