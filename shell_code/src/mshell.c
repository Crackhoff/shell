#include <stdio.h>
#include <sys/types.h>
#include "config.h"
#include "siparse.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <builtins.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <bits/waitflags.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define num_finished 5
typedef int (*builtin)(char **);
// typedef struct fg_procs fg_procs, *
typedef struct fg_procs fg_procs;
typedef struct fg_procs_collection fg_procs_collection;
// struct fg_procs
// {
// 	pid_t pid;
// 	fg_procs *next;
// 	fg_procs *prev;
// };

struct fg_procs_collection
{
	pid_t arr[MAX_LINE_LENGTH / 2];
	int size;
};

struct fg_procs_operations
{
	void (*add)(fg_procs_collection *fg, pid_t pid);
	void (*remove)(fg_procs_collection *fg, pid_t pid);
	int (*member)(fg_procs_collection *fg, pid_t pid);
};

void fg_procs_add(fg_procs_collection *fg, pid_t pid)
{
	fg->arr[fg->size] = pid;
	fg->size++;
}

void fg_procs_killall(fg_procs_collection *fg)
{
	for (int i = 0; i < fg->size; i++)
	{
		kill(fg->arr[i], SIGINT);
	}
	printf("pre\n");
	fg->size = 0;
	printf("after\n");
}

void fg_procs_removeall(fg_procs_collection *fg)
{
	fg->size = 0;
	// pid_t pids[fg->size];
	// for (int i = 0; i < fg->size; i++)
	// {
	// 	pids[i] = fg->arr[i];
	// }
	// for (int i = 0; i < fg->size; i++)
	// {
	// 	fg_procs_remove(fg, pids[i]);
	// }
}
int fg_procs_remove(fg_procs_collection *fg, pid_t pid)
{
	for (int i = 0; i < fg->size; i++)
	{
		if (fg->arr[i] == pid)
		{
			fg->arr[i] = fg->arr[fg->size - 1];
			fg->size--;
			return 1;
		}
	}
	return 0;
}

int fg_procs_member(fg_procs_collection *fg, pid_t pid)
{
	for (int i = 0; i < fg->size; i++)
	{
		if (fg->arr[i] == pid)
			return 1;
	}
	return 0;
}

char buf[4 * 1024 + 5]; // buf is pointing to the beginning of the buffer
int offset = 0;			// offset is the number of bytes from the beginning of the buffer
int unused_line = 0;
int used = 0;
char *curr = buf; // curr is pointing to the beginning of the unused part of the buffer
volatile int num_child = 0;
size_t line_len;
int num_finished_curr = 0;
char finished[num_finished][1024];
fg_procs_collection *fg = NULL;
sigset_t empty_mask;
sigset_t child_mask;

// HANDLERY TUTAJ
void handlerChld(sig) int sig;
{
	// printf("chld\n");
	int wstatus;
	// uwaga na dzieci z foregroundu!
	// sprawdzic, czy to dzieciak z foregroundu
	// zmniejszyc licznik dzieci na ktore czekamy
	// wpp
	// ale jeden waitpid nie starczy, trzeba w pętli
	while (1)
	{
		int pid = waitpid(-1, &wstatus, WNOHANG);
		// printf("waited %d\n", pid);
		if (pid == 0 || pid == -1)
			return;
		else if (pid != -1)
		{
			// printf("waited\n");
			if (fg_procs_member(fg, pid))
			{
				fg_procs_remove(fg, pid);
				num_child--;
			}
			else
			{
				if (WIFEXITED(wstatus))
					sprintf(finished[num_finished_curr % num_finished], "Background process (%d) terminated. (exited with status (%d))\n", pid, WEXITSTATUS(wstatus));
				else if (WIFSIGNALED(wstatus))
					sprintf(finished[num_finished_curr % num_finished], "Background process (%d) terminated. (killed by signal (%d))\n", pid, WTERMSIG(wstatus));
				else if (WIFSTOPPED(wstatus))
					sprintf(finished[num_finished_curr % num_finished], "Background process (%d) stopped. (stopped by signal (%d))\n", pid, WSTOPSIG(wstatus));
				else if (WIFCONTINUED(wstatus))
					sprintf(finished[num_finished_curr % num_finished], "Background process (%d) continued.\n", pid);
				num_finished_curr++;
			}
		}
	}
	// dodaj do jakeigos stringa info o zakonczonym procesie
	// domyslnie nie mozna w trakcie handlera wolac innego sygnału
}

builtin builtins(char *name, int *found)
{
	int idx = 0;
	while (builtins_table[idx].name != NULL)
	{
		if (strcmp(builtins_table[idx].name, name) == 0)
		{
			*found = 1;
			return builtins_table[idx].fun;
		}
		idx++;
	}
	*found = -1;
	return NULL;
}
int closePipes(int pipes[2][2], int parity)
{
	close(pipes[parity][0]);
	close(pipes[parity][1]);
	return 0;
}
int closeAllPipes(int pipes[2][2])
{
	close(pipes[0][0]);
	close(pipes[0][1]);
	close(pipes[1][0]);
	close(pipes[1][1]);
	return 0;
}

int manageFile(char *filename, int flags, int isout)
{
	int file = open(filename, flags, S_IRUSR | S_IWUSR);
	if (file == -1)
	{
		if (errno == EACCES)
			fprintf(stderr, "%s: permission denied\n", filename);
		else if (errno == ENOENT)
		{
			fprintf(stderr, "%s: no such file or directory\n", filename);
		}
		else
			fprintf(stderr, "unable to open the file\n");
		exit(EXEC_FAILURE);
	}
	dup2(file, isout);
	close(file);
	return 0;
}

int manageRedirs(redirseq *redir)
{

	if (redir == NULL)
	{
		return 0;
	}
	redir->prev->next = NULL;
	while (redir != NULL)
	{
		int way = redir->r->flags;
		char *f = redir->r->filename;
		if (IS_RIN(way))
		{
			int file = manageFile(f, O_RDONLY, 0);
		}
		else if (IS_ROUT(way))
		{
			int file = manageFile(f, O_WRONLY | O_CREAT | O_TRUNC, 1);
		}
		else if (IS_RAPPEND(way))
		{
			int file = manageFile(f, O_WRONLY | O_CREAT | O_APPEND, 1);
		}
		else
		{
			fprintf(stderr, "unknown redirection\n");
			exit(EXEC_FAILURE);
		}
		redir = redir->next;
	}
	return 0;
}

int manageArgs(argseq *argsq, char **argv)
{
	argseq *firstarg = argsq;
	int idx = 0;
	do
	{
		argv[idx] = argsq->arg;
		idx++;
		argsq = argsq->next;
	} while (argsq != firstarg);
	argv[idx] = 0;
	return 0;
}

int exePl(pipeline *pl)
{
	pid_t pids[MAX_LINE_LENGTH / 2];
	int pipes[2][2];
	int parity = 0;
	int isFirst = 1;
	int isLast = 0;
	int isBg = pl->flags & INBACKGROUND;
	commandseq *first_com = pl->commands;
	commandseq *comsq = pl->commands;
	pipe(pipes[0]);
	pipe(pipes[1]);
	do
	{
		if (comsq->next == first_com)
			isLast = 1;
		command *cmd = comsq->com;
		sigprocmask(SIG_BLOCK, &child_mask, NULL);
		pid_t p = fork();
		if (p == 0)
		{
			// imma child, i have to execute the command
			if (isBg)
			{
				setsid();
			}
			signal(SIGINT, SIG_DFL);
			if (!isFirst)
			{
				dup2(pipes[parity][0], 0);
			}
			if (!isLast)
			{
				dup2(pipes[1 - parity][1], 1);
			}
			closeAllPipes(pipes);
			char *argv[MAX_LINE_LENGTH + 1];
			// wypelniamy tablice argv!
			manageArgs(cmd->args, argv);
			manageRedirs(cmd->redirs);
			execvp(argv[0], argv);
			// called only in the case of faliure
			if (errno == ENOENT)
			{
				fprintf(stderr, "%s: no such file or directory\n", argv[0]);
			}
			else if (errno == EACCES)
			{
				fprintf(stderr, "%s: permission denied\n", argv[0]);
			}
			else
			{
				fprintf(stderr, "%s: exec error\n", argv[0]);
			}
			// else if(errno == )
			// no error:
			exit(EXEC_FAILURE);
		}
		else if (p < 0)
		{
			// handling fork error
			// properly exiting the program
			fprintf(stderr, "fork error\n");
			exit(EXEC_FAILURE);
		}
		// parent
		if (!isBg)
		{
			fg_procs_add(fg, p);
			num_child++;
			// printf("added %d\n", p);
			sigprocmask(SIG_UNBLOCK, &child_mask, NULL);
		}
		// fg.add()
		isFirst = 0;
		closePipes(pipes, parity);
		pipe(pipes[parity]);
		parity = 1 - parity;
		comsq = comsq->next;
	} while (comsq != first_com);
	closeAllPipes(pipes);
	// for (int i = 0; i < children; i++)
	// block sigchild
	// printf("num_child: %d\n", num_child);
	sigprocmask(SIG_BLOCK, &child_mask, NULL);
	while (num_child > 0)
	{
		/*
		czy ten pid jest procesem z foregroundu?
		struktura na pidy -> fg-procs
		insert, czy member, delete -> ideal
		tablica starczy, ale abstrakcja, a nie łopatologicznie, osobna funkcja

		handler na SIGCHLD
		zamiast tego while:

		pause()
		sygnały ją przerywają po prostu

		BLOKOWANIE SYGNAŁU SIGCHLD PRZED FORKIEM

		I ODBLOKOWANIE PO USTAWIENIU RZECZY ZWIĄZANYCH Z DZIECKIEM

		dookoła pauzy nie da się zablokować,

		ale sigsuspend jest rozwiązaniem
		w odróżnieniu od pauzy, czeka na konkretną maskę
		sigsuspend(empty_mask) w środku pętli
		*/
		// pause();
		sigsuspend(&empty_mask);
		// while (wait(NULL) > 0)
		// 	;
		// printf("waited\n");
	}
	// unblock sigchild
	sigprocmask(SIG_UNBLOCK, &child_mask, NULL);
}

int exeLn(pipelineseq *ln)
{
	/*

	*/
	pipelineseq *first_pipeline = ln;
	do
	{

		if (ln->pipeline->commands->next == ln->pipeline->commands)
		{
			command *cmd = ln->pipeline->commands->com;
			int is_builtin = 0;
			builtin f = builtins(cmd->args->arg, &is_builtin);
			if (is_builtin == 1)
			{
				char *argv[MAX_LINE_LENGTH / 2 + 1];
				int idx = 0;
				argseq *argseq = cmd->args;
				do
				{
					argv[idx] = argseq->arg;
					idx++;
					argseq = argseq->next;
				} while (argseq != cmd->args);
				argv[idx] = 0;
				f(argv);
				return 0;
			}
			exePl(ln->pipeline);
		}
		else
			exePl(ln->pipeline);
		ln = ln->next;
	} while (first_pipeline != ln);
	return 0;
}
void parsein()
{
	if (memchr(curr, '\n', line_len) != NULL)
	{
		while (memchr(curr, '\n', line_len) != NULL)
		{
			char *end = memchr(curr, '\n', line_len);
			if (unused_line)
			{
				curr = end + 1;
				unused_line = 0;
				continue;
			}
			*end = 0;
			offset = end - curr + 1;
			if (offset > MAX_LINE_LENGTH)
			{
				curr = end + 1;
				fprintf(stderr, SYNTAX_ERROR_STR "\n");
				continue;
			}
			pipelineseq *ln = parseline(curr);
			curr = end + 1;
			line_len = MIN(used - (curr - buf), MAX_LINE_LENGTH);
			if (ln == NULL)
			{
				fprintf(stderr, SYNTAX_ERROR_STR "\n");
			}
			else if (pickfirstcommand(ln) == NULL)
			{
				continue;
			}
			else
			{
				// fprintf(stderr, "executing\n");
				exeLn(ln);
			}
		}
	}
	else
	{
		if (line_len == MAX_LINE_LENGTH)
		{
			if (!unused_line)
				fprintf(stderr, SYNTAX_ERROR_STR "\n");
			unused_line = 1;
			curr += line_len;
		}
		// else the bufor will be moved
		// and a space for new read will be available
	}
	offset = used - (curr - buf);
	// copy memory each time instead of when needed, that was error prone
	// if (sizeof(buf) - used <= MAX_LINE_LENGTH)
	// {
	// not enough space, move to the beginning
	memmove(buf, curr, offset);
	curr = buf;
	used = offset;
	// }
}

int main(int argc, char *argv[])
{
	int finished_reading = 0;
	fg_procs_collection emptyfg;
	emptyfg.size = 0;
	fg = &emptyfg;
	struct sigaction sachld;
	sachld.sa_handler = handlerChld;
	sachld.sa_flags = 0;
	sigemptyset(&sachld.sa_mask);
	sigaction(SIGCHLD, &sachld, NULL);
	signal(SIGINT, SIG_IGN);
	command *com;
	sigemptyset(&empty_mask);
	sigfillset(&child_mask);
	// sigaction rejestruje handlery i mozna w nim zablokowac sygnaly na czas wykonywania sygnalu
	// domyslnie blokuje ten sygnal ktory sie wykonuje

	int in_from_terminal = isatty(STDOUT_FILENO); // either this or fstat (might change it later)
	while (1)
	{
		//-> wypisz;
		if (in_from_terminal == 1)
		{
			// wypisz zakonczone procesy z backgroundu
			// usun notatki
			for (int i = 0; i < MIN(num_finished_curr, num_finished); i++)
			{
				printf("%s", finished[i]);
			}
			num_finished_curr = 0;
			printf("%s", PROMPT_STR);
			fflush(stdout);
		}
		//-> wpisz
		int t = 0;
		t = read(0, curr + offset * sizeof(char), MAX_LINE_LENGTH); // or MAX_LINE_LENGTH as 3rd argument?
		// if read -1 => mógł być sygnał, trzeba sprawdzić
		// errno EINTR
		// powielić reada spróbuj jeszcze raz
		// ale, waitpida można zrobić to w handlerze!
		// printf("t: %d\n", t);
		// printf("%s\n end \n", curr + offset, t);
		// printf("%d %d\n", t, curr + offset + t - 1);
		// printf("koniec \n");
		// printf("eof corr: %d\n", EOF);
		// fflush(stdout);
		if (t == -1)
		{
			if (errno == EINTR)
				continue;
		}
		used += t;
		line_len = MIN(used - (buf - curr), MAX_LINE_LENGTH);
		if (t == 0)
		{
			if (in_from_terminal == 1)
				printf("\n");
			break;
		}
		parsein();
	}
	// free(fg);
	return 0;
}
/*
	pliki i przekierowanie strumieni
	> zawsze nadpisanie pliku (truncate, create)
	>> dopisanie do pliku (append, create)
	< otwórz plik (open) [do odczytu]

	mogą być równocześnie

	PIPY
	każda komenda - osobny proces

	połączone pipami
	pierwsze i ostatnie - odziedziczone od rodzica


	NAJPIERW ROBI PIPY

	potem forki mordo

	POTEM ROBI REDIR EXEC I MOŻE SIĘ TO ZMIENIĆ

	*/

/*
ładne pajplajny, bez builtinów, jeżeli jest więcej niż jedna komenda,
albo jakiekolwiek przekierowanie

fork wykonuje się w pętli, tworzy wszystkie forki, a dopiero potem zaczyna ich wykonywanie

CZEKAMY NA WSZYSTKIE PROCESY i to nie za pomocą czekania na każde po kolei
policzyć waitpidy, jeżeli jest ich tyle ile forków, koniec
for(int i=0;i<num_child;i++)
	waitpid(-1)

nie trzeba wszystkich pipów na raz

shell tworzy pierwszy pipe

forkuje

tworzy kolejnego pipe

forkuje, dziedziczy poprzednie 2

patrzy dalej, ale ten pierwszy pipe jest już odziedziczony i niepotrzebny!
ODPINA SIĘ OD NIEGO!

4 dodatkowe deskryptory i jest git


ŚREDNIKII
parser po prostu to dzieli, można je wykonywać osobno

*/
// printparsedline(ln);

// #REDIR EXEC
//  tutaj zamiana wejścia/ wyjścia
// blok przekierowań dzieje się tutaj w pętli
// parser przyjmie je wszystkie
// podmienić programowi 0 i 1, 2 zostaje jak była