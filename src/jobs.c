#include "jobs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>

static Job jobs[MAX_JOBS];
static int next_job_number = 1;
static int job_count = 0;

void jobs_init(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        jobs[i].job_number = 0;
        jobs[i].pid = 0;
        jobs[i].command = NULL;
        jobs[i].state = JOB_COMPLETED;
        jobs[i].is_background = false;
    }
    next_job_number = 1;
    job_count = 0;
}

int jobs_add(pid_t pid, const char *command, bool is_background) {
    if (job_count >= MAX_JOBS) {
        return -1; // No space for more jobs
    }
    
    // Find an empty slot
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].state == JOB_COMPLETED) {
            jobs[i].job_number = next_job_number++;
            jobs[i].pid = pid;
            jobs[i].command = command ? strdup(command) : NULL;
            jobs[i].state = is_background ? JOB_RUNNING : JOB_RUNNING;
            jobs[i].is_background = is_background;
            job_count++;
            return jobs[i].job_number;
        }
    }
    
    return -1; // No empty slot found
}

void jobs_remove(int job_number) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].job_number == job_number) {
            free(jobs[i].command);
            jobs[i].job_number = 0;
            jobs[i].pid = 0;
            jobs[i].command = NULL;
            jobs[i].state = JOB_COMPLETED;
            jobs[i].is_background = false;
            job_count--;
            break;
        }
    }
}

Job *jobs_get(int job_number) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].job_number == job_number) {
            return &jobs[i];
        }
    }
    return NULL;
}

Job *jobs_get_by_pid(pid_t pid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid == pid && jobs[i].state != JOB_COMPLETED) {
            return &jobs[i];
        }
    }
    return NULL;
}

void jobs_check_completed(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].state == JOB_RUNNING && jobs[i].is_background) {
            int status;
            pid_t result = waitpid(jobs[i].pid, &status, WNOHANG);
            
            if (result > 0) {
                // Process has completed
                if (WIFEXITED(status)) {
                    printf("%s with pid %d exited normally\n", 
                           jobs[i].command ? jobs[i].command : "Command", 
                           jobs[i].pid);
                    fflush(stdout);
                } else if (WIFSIGNALED(status)) {
                    printf("%s with pid %d exited abnormally\n", 
                           jobs[i].command ? jobs[i].command : "Command", 
                           jobs[i].pid);
                    fflush(stdout);
                }
                
                // Remove the completed job
                jobs_remove(jobs[i].job_number);
            }
        }
    }
}

void jobs_print_job(int job_number, pid_t pid) {
    printf("[%d] %d\n", job_number, pid);
}

int jobs_get_next_number(void) {
    return next_job_number;
}

void jobs_cleanup(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        free(jobs[i].command);
        jobs[i].command = NULL;
    }
}

void jobs_kill_all(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].state != JOB_COMPLETED && jobs[i].pid > 0) {
            kill(jobs[i].pid, SIGKILL);
        }
    }
}

// Part E functions

void jobs_list_activities(void) {
    // Create array of active jobs for sorting
    Job *active_jobs[MAX_JOBS];
    int active_count = 0;
    
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].state != JOB_COMPLETED) {
            active_jobs[active_count++] = &jobs[i];
        }
    }
    
    // Sort by command name (lexicographically)
    for (int i = 0; i < active_count - 1; i++) {
        for (int j = i + 1; j < active_count; j++) {
            const char *name1 = active_jobs[i]->command ? active_jobs[i]->command : "unknown";
            const char *name2 = active_jobs[j]->command ? active_jobs[j]->command : "unknown";
            if (strcmp(name1, name2) > 0) {
                Job *temp = active_jobs[i];
                active_jobs[i] = active_jobs[j];
                active_jobs[j] = temp;
            }
        }
    }
    
    // Print sorted results
    for (int i = 0; i < active_count; i++) {
        const char *state_str = (active_jobs[i]->state == JOB_RUNNING) ? "Running" : "Stopped";
        const char *cmd_name = active_jobs[i]->command ? active_jobs[i]->command : "unknown";
        printf("[%d] : %s - %s\n", active_jobs[i]->pid, cmd_name, state_str);
    }
}

int jobs_send_signal(int job_number, int signal_num) {
    Job *job = jobs_get(job_number);
    if (!job) {
        printf("No such process found\n");
        return -1;
    }
    
    // Take signal number modulo 32
    int actual_signal = signal_num % 32;
    
    if (kill(job->pid, actual_signal) == 0) {
        printf("Sent signal %d to process with pid %d\n", signal_num, job->pid);
        return 0;
    } else {
        printf("No such process found\n");
        return -1;
    }
}

int jobs_bring_to_foreground(int job_number) {
    Job *job = jobs_get(job_number);
    if (!job) {
        printf("No such job\n");
        return -1;
    }
    
    // Print the command being brought to foreground
    const char *cmd_name = job->command ? job->command : "unknown";
    printf("%s\n", cmd_name);
    
    // If job is stopped, resume it
    if (job->state == JOB_STOPPED) {
        if (kill(job->pid, SIGCONT) != 0) {
            printf("No such job\n");
            return -1;
        }
        job->state = JOB_RUNNING;
    }
    
    // Wait for the job to complete or stop again
    int status;
    pid_t result = waitpid(job->pid, &status, WUNTRACED);
    
    if (result > 0) {
        if (WIFSTOPPED(status)) {
            // Job was stopped again
            job->state = JOB_STOPPED;
            printf("[%d] Stopped %s\n", job->job_number, cmd_name);
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            // Job completed
            jobs_remove(job_number);
        }
    }
    
    return 0;
}

int jobs_resume_background(int job_number) {
    Job *job = jobs_get(job_number);
    if (!job) {
        printf("No such job\n");
        return -1;
    }
    
    if (job->state == JOB_RUNNING) {
        printf("Job already running\n");
        return 0;
    }
    
    if (job->state == JOB_STOPPED) {
        if (kill(job->pid, SIGCONT) == 0) {
            job->state = JOB_RUNNING;
            const char *cmd_name = job->command ? job->command : "unknown";
            printf("[%d] %s &\n", job->job_number, cmd_name);
            return 0;
        } else {
            printf("No such job\n");
            return -1;
        }
    }
    
    return -1;
}

int jobs_get_most_recent_job(void) {
    int most_recent = -1;
    int highest_job_num = 0;
    
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].state != JOB_COMPLETED && jobs[i].job_number > highest_job_num) {
            highest_job_num = jobs[i].job_number;
            most_recent = jobs[i].job_number;
        }
    }
    
    return most_recent;
}

void jobs_set_stopped(int job_number) {
    Job *job = jobs_get(job_number);
    if (job) {
        job->state = JOB_STOPPED;
    }
}

void jobs_set_running(int job_number) {
    Job *job = jobs_get(job_number);
    if (job) {
        job->state = JOB_RUNNING;
    }
}

