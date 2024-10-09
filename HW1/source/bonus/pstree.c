#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>

#define PROC_PREFIX "/proc"
#define MAX_PROCESSES 100005
#define MAX_CHILDREN 100
#define MAX_THREADS 100
#define MAX_NAME_LEN 256

typedef struct process_node
{
    int pid;
    int ppid;
    char name[MAX_NAME_LEN + 2]; // Process name, 256 + 2 (brackets)
    char cmdline[1024];
    int thread_count; // Number of threads
    struct process_node *children[MAX_CHILDREN];
    int child_count; // Number of children
} process_node_t;

process_node_t *process_tree[MAX_PROCESSES];

int process_count = 0;
int show_threads = 1;
int use_thread_names = 0;
int show_pid = 0;
int show_compact = 1;
int show_cmdline = 0;
int sort_by_pid = 0;
int align_mode = 1;
int show_ascii_tree = 0;
int branch_bar_recorder[1000];

char *branch_chars_default[] = {"├", "└", "│", "─", "┬"};
char *branch_chars_ascii[] = {"|", "`", "|", "-", "+"};
char **branch_chars = branch_chars_default;

void clean_process_name(char *name)
{
    if (name[0] == '(' || name[0] == '[')
    {
        name[strlen(name) - 1] = '\0';
        memmove(name, name + 1, strlen(name));
    }
}

void read_process_cmdline(int pid, process_node_t *node)
{
    char path[MAX_NAME_LEN];
    sprintf(path, PROC_PREFIX "/%d/cmdline", pid);
    FILE *cmdline_file = fopen(path, "r");
    if (cmdline_file)
    {
        size_t len = fread(node->cmdline, 1, sizeof(node->cmdline) - 1, cmdline_file);
        fclose(cmdline_file);

        if (len > 0)
        {
            node->cmdline[len] = '\0';
            for (size_t i = 0; i < len; i++)
            {
                if (node->cmdline[i] == '\0')
                {
                    node->cmdline[i] = ' ';
                }
            }

            len = strlen(node->cmdline);
            while (len > 0 && isspace((unsigned char)node->cmdline[len - 1]))
            {
                node->cmdline[--len] = '\0';
            }
        }
        else
        {
            strcpy(node->cmdline, "()");
        }
    }
    else
    {
        strcpy(node->cmdline, "()");
    }
}

void read_process_threads(int pid, process_node_t *node)
{
    char path[MAX_NAME_LEN];
    DIR *task_dir;
    struct dirent *entry;

    sprintf(path, PROC_PREFIX "/%d/task", pid);
    task_dir = opendir(path);
    if (!task_dir)
        return;

    while ((entry = readdir(task_dir)) != NULL)
    {
        if (isdigit(entry->d_name[0]))
        {
            int tid = atoi(entry->d_name);
            if (tid != pid)
            {
                sprintf(path, PROC_PREFIX "/%d/task/%d/stat", pid, tid);
                FILE *stat_file = fopen(path, "r");
                if (stat_file)
                {
                    char thread_name[256];
                    fscanf(stat_file, "%*d %s", thread_name);
                    fclose(stat_file);

                    clean_process_name(thread_name);

                    process_node_t *thread_node = malloc(sizeof(process_node_t));
                    thread_node->pid = tid;
                    thread_node->ppid = pid;
                    if (use_thread_names)
                    {
                        snprintf(thread_node->name, sizeof(thread_node->name), "{%s}", thread_name);
                    }
                    else
                    {
                        snprintf(thread_node->name, sizeof(thread_node->name), "{%s}", node->name);
                    }

                    thread_node->child_count = 0;
                    thread_node->thread_count = 0;

                    node->children[node->child_count++] = thread_node;
                }
            }
        }
    }
    closedir(task_dir);
}

void read_process_info(int pid)
{
    char path[MAX_NAME_LEN], name[MAX_NAME_LEN];
    FILE *stat_file;
    int ppid;

    sprintf(path, PROC_PREFIX "/%d/stat", pid);
    stat_file = fopen(path, "r");
    if (!stat_file)
        return;

    fscanf(stat_file, "%d %s %*c %d", &pid, name, &ppid);
    fclose(stat_file);

    clean_process_name(name);

    process_node_t *node = malloc(sizeof(process_node_t));
    node->pid = pid;
    node->ppid = ppid;
    strncpy(node->name, name, sizeof(node->name));
    node->thread_count = 0;
    node->child_count = 0;
    node->cmdline[0] = '\0';

    process_tree[pid] = node;
    process_count++;

    if (show_threads)
    {
        read_process_threads(pid, node);
    }

    if (show_cmdline)
    {
        read_process_cmdline(pid, node);
    }
}

int compare_by_name(const void *a, const void *b)
{
    process_node_t *node_a = *(process_node_t **)a;
    process_node_t *node_b = *(process_node_t **)b;
    return strcmp(node_a->name, node_b->name);
}

int compare_by_pid(const void *a, const void *b)
{
    process_node_t *node_a = *(process_node_t **)a;
    process_node_t *node_b = *(process_node_t **)b;
    return node_a->pid - node_b->pid;
}

void sort_children(process_node_t *node)
{
    if (node->child_count > 1)
    {
        qsort(
            node->children,
            node->child_count,
            sizeof(process_node_t *),
            sort_by_pid ? compare_by_pid : compare_by_name);
    }
}

void read_all_processes()
{
    DIR *proc_dir;
    struct dirent *entry;

    proc_dir = opendir(PROC_PREFIX);
    if (!proc_dir)
    {
        perror("Cannot open /proc");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(proc_dir)) != NULL)
    {
        if (isdigit(entry->d_name[0]))
        {
            int pid = atoi(entry->d_name);
            read_process_info(pid);
        }
    }
    closedir(proc_dir);
}

void build_process_tree()
{
    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        if (process_tree[i] != NULL)
        {
            int ppid = process_tree[i]->ppid;
            if (ppid > 0 && process_tree[ppid] != NULL)
            {
                process_tree[ppid]->children[process_tree[ppid]->child_count++] = process_tree[i];
            }
        }
    }

    for (int i = 0; i < MAX_PROCESSES; i++)
    {
        if (process_tree[i] != NULL)
        {
            sort_children(process_tree[i]);
        }
    }
}

int count_same_name_children(process_node_t *node, int start_idx, char *name)
{
    int count = 0;
    for (int i = start_idx; i < node->child_count; i++)
    {
        if (strcmp(node->children[i]->name, name) == 0 && node->children[i]->child_count == 0)
        {
            count++;
        }
    }
    return count;
}

void print_bar(int is_print)
{
    if (!align_mode)
        printf("  ");

    if (is_print)
        printf(branch_chars[2]);
    else
        printf(" ");

    if (!align_mode)
        printf(" ");
}

void print_process_tree(process_node_t *node, int depth, int is_last_child, int is_first_child)
{
    // Print the tree structure
    char *branch_prefix = branch_chars[0];
    if (is_last_child == 1)
    {
        branch_prefix = branch_chars[1];
    }

    if (align_mode)
    {
        branch_bar_recorder[depth + 1] = is_last_child ? 0 : 1;
    }
    else
    {
        branch_bar_recorder[depth] = is_last_child ? 0 : 1;
    }

    if (!(align_mode && is_first_child))
        for (int i = 0; i < depth; i++)
        {
            print_bar(branch_bar_recorder[i]);
        }

    char *name_p = node->name;
    char name[260] = "";

    if (node->cmdline[0] != '\0')
    {
        // Remove the absolute path of the command
        char *cmdline = node->cmdline;
        // Get the last space position
        char *first_name_occur = strstr(cmdline, " ");
        char *last_slash = NULL;

        if (first_name_occur)
        {
            // find the last slash before the first space
            last_slash = first_name_occur;
            while (last_slash > cmdline && *last_slash != '/')
            {
                last_slash--;
            }

            if (*last_slash == '/')
            {
                last_slash++; // skip the slash
            }
            else
            {
                last_slash = cmdline;
            }

            // copy the name
            name_p = last_slash;
            strncpy(name, name_p, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
        }
        else
        {
            name_p = cmdline;
            strncpy(name, name_p, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
        }
    }
    else
    {
        // use the name if cmdline is empty
        strncpy(name, name_p, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    }

    if (align_mode && is_first_child)
    {
        branch_prefix = branch_chars[4];
        if (is_last_child)
        {
            branch_prefix = branch_chars[3];
        }
    }
    char *sep = branch_chars[3];

    char depth_prefix[20] = "";
    int depth_prefix_len = 0;
    // format depth prefix
    if (depth != -1)
    {
        if (align_mode)
        {
            if (is_first_child)
            {
                snprintf(depth_prefix, sizeof(depth_prefix), "%s%s%s", sep, branch_prefix, sep);
                depth_prefix_len = 3;
            }
            else
            {
                snprintf(depth_prefix, sizeof(depth_prefix), " %s%s", branch_prefix, sep);
                depth_prefix_len = 3;
            }
        }
        else
        {
            snprintf(depth_prefix, sizeof(depth_prefix), "  %s%s", branch_prefix, sep);
            depth_prefix_len = 4;
        }
    }

    char output[1024] = "";
    if (name == NULL)
    {
        fprintf(stderr, "Error: name is NULL\n");
        return;
    }

    // debug use
    // for (int i = 0; i < strlen(name); i++)
    // {
    //     printf("%c (%d)\n", name[i], (unsigned char)name[i]);
    // }

    if (show_pid)
        snprintf(output, sizeof(output), "%s(%d)", name, node->pid);
    else
        snprintf(output, sizeof(output), "%s", name);

    int offset = 0;

    if (align_mode)
    {
        // all subnode adding depth(offset) by output length
        if (depth != -1)
            offset += depth_prefix_len;
        else
            offset += 1;
        offset += strlen(output);
    }
    else
    {
        offset += 1;
    }

    printf("%s%s", depth_prefix, output);

    if (align_mode)
    {
        if (node->child_count == 0)
        {
            printf("\n");
        }
    }
    else
    {
        printf("\n");
    }

    for (int i = 0; i < node->child_count; i++)
    {
        int same_name_count = 1;

        if (show_compact)
        {
            same_name_count = count_same_name_children(node, i, node->children[i]->name);
        }

        int fisrt_child_flag = 0;
        if (same_name_count > 1)
        {
            i += same_name_count - 1; // skip same name children

            // update node->children[i]->name
            char temp_name[260] = "";

            snprintf(temp_name, sizeof(temp_name), "%d*[%s]", same_name_count, node->children[i]->name);
            snprintf(node->children[i]->name, sizeof(node->children[i]->name), "%s", temp_name);

            fisrt_child_flag = same_name_count == i + 1;
        }
        print_process_tree(node->children[i], depth + offset, i == node->child_count - 1, i == 0 || fisrt_child_flag);
    }
}

int main(int argc, char *argv[])
{
    // handle the arguments
    for (int i = 1; i < argc; i++)
    {
        // debug use
        // printf("argv[%d]: %s\n", i, argv[i]);

        if (!strchr(argv[i], '-'))
        {
            continue;
        }

        if (strchr(argv[i], 'T'))
        {
            show_threads = 0;
        }
        if (strchr(argv[i], 't'))
        {
            use_thread_names = 1;
        }
        if (strchr(argv[i], 'p'))
        {
            show_pid = 1;
        }
        if (strchr(argv[i], 'c'))
        {
            show_compact = 0;
        }
        if (strchr(argv[i], 'a'))
        {
            show_cmdline = 1;
            align_mode = 0;
        }
        if (strchr(argv[i], 'n'))
        {
            sort_by_pid = 1;
        }
        if (strchr(argv[i], 'A'))
        {
            branch_chars = branch_chars_ascii;
            show_ascii_tree = 1;
        }
    }

    read_all_processes();
    build_process_tree();

    if (process_tree[1] != NULL)
    {
        print_process_tree(process_tree[1], -1, 1, 1);
    }

    return 0;
}
