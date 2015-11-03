#include <lixs/event_mgr.hh>
#include <lixs/mstore/store.hh>
#include <lixs/os_linux/epoll.hh>
#include <lixs/unix_sock_server.hh>
#include <lixs/virq_handler.hh>
#include <lixs/xenbus.hh>
#include <lixs/xenstore.hh>

#include <csignal>
#include <cstdio>
#include <fcntl.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


struct lixs_conf {
    lixs_conf(int argc, char** argv)
        : error(false),
        help(false),
        daemonize(false),
        xenbus(false),
        virq_dom_exc(false),
        log_to_file(false),
        write_pid_file(false),

        pid_file("/var/run/xenstored.pid"),
        log_file("/var/log/xen/lixs.log"),
        unix_socket_path("/run/xenstored/socket"),
        unix_socket_ro_path("/run/xenstored/socket_ro"),
        cmd(argv[0])
    {
        /* FIXME: allow to specify socket paths */
        /* TODO: Improve configurability */

        const char *short_opts = "hD";
        const struct option long_opts[] = {
            { "help"               , no_argument       , NULL , 'h' },
            { "daemon"             , no_argument       , NULL , 'D' },
            { "xenbus"             , no_argument       , NULL , 'x' },
            { "virq-dom-exc"       , no_argument       , NULL , 'i' },
            { "pid-file"           , required_argument , NULL , 'p' },
            { "log-file"           , required_argument , NULL , 'l' },
            { NULL , 0 , NULL , 0 }
        };

        int opt;
        int opt_index;

        while (1) {
            opt = getopt_long(argc, argv, short_opts, long_opts, &opt_index);

            if (opt == -1) {
                break;
            }

            switch (opt) {
                case 'h':
                    help = true;
                    break;

                case 'D':
                    daemonize = true;
                    log_to_file = true;
                    write_pid_file = true;
                    break;

                case 'x':
                    xenbus = true;
                    break;

                case 'i':
                    virq_dom_exc = true;
                    break;

                case 'p':
                    write_pid_file = true;
                    pid_file = std::string(optarg);
                    break;

                case 'l':
                    log_to_file = true;
                    log_file = std::string(optarg);
                    break;

                default:
                    error = true;
                    break;
            }
        }

        while (optind < argc) {
            error = true;

            printf("%s: invalid argument \'%s\'\n", cmd.c_str(), argv[optind]);
            optind++;
        }
    }

    void print_usage() {
        printf("Usage: %s [OPTION]...\n", cmd.c_str());
        printf("\n");
        printf("Options:\n");
        printf("  -D, --daemon           Run in background\n");
        printf("      --xenbus           Enable attaching to xenbus\n");
        printf("                         [default: Disabled]\n");
        printf("      --virq-dom-exc     Enable handling of VIRQ_DOM_EXC\n");
        printf("                         [default: Disabled]\n");
        printf("      --pid-file <file>  Write process pid to file\n");
        printf("                         [default: /var/run/xenstored.pid]\n");
        printf("      --log-file <file>  Write log output to file\n");
        printf("                         [default: /var/log/xen/lixs.log]\n");
        printf("  -h, --help             Display this help and exit\n");
    }


    bool error;

    bool help;
    bool daemonize;
    bool xenbus;
    bool virq_dom_exc;
    bool log_to_file;
    bool write_pid_file;

    std::string pid_file;
    std::string log_file;
    std::string unix_socket_path;
    std::string unix_socket_ro_path;

private:
    std::string cmd;
};


static lixs::event_mgr* emgr_ptr;

static void signal_handler(int sig)
{
    if (sig == SIGINT) {
        printf("[LiXS]: Got SIGINT, stopping...\n");
        emgr_ptr->disable();
    }
}

static bool setup_logging(lixs_conf& conf)
{
    /* Reopen stdout first so that worst case we can still log to stderr */
    if (freopen(conf.log_file.c_str(), "w", stdout) == NULL) {
        goto out_err;
    }
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (freopen(conf.log_file.c_str(), "w", stderr) == NULL) {
        goto out_err;
    }
    setvbuf(stderr, NULL, _IOLBF, 0);

    return false;

out_err:
    fprintf(stderr, "[LiXS]: Failed to redirect output to file: %d\n", errno);
    return true;
}

static int daemonize(lixs_conf& conf)
{
    /* If log to file is enabled we cannot let daemon() handle file descriptors
     * (it would close the log files) but we still need to close stdin
     */
    if (conf.log_to_file) {
        fclose(stdin);
    }

    if (daemon(1, conf.log_to_file ? 1 : 0)) {
        fprintf(stderr, "[LiXS]: Failed to daemonize: %d\n", errno);
        return true;
    }

    return false;
}

int main(int argc, char** argv)
{
    lixs_conf conf(argc, argv);

    if (conf.error) {
        conf.print_usage();
        return -1;
    }

    if (conf.help) {
        conf.print_usage();
        return 0;
    }

    if (conf.write_pid_file) {
        FILE* pidf = fopen(conf.pid_file.c_str(), "w");
        fprintf(pidf, "%d", getpid());
        fclose(pidf);
    }

    if (conf.log_to_file) {
        if (setup_logging(conf)) {
            return -1;
        }
    }

    if (conf.daemonize) {
        if (daemonize(conf)) {
            return -1;
        }
    }

    printf("[LiXS]: Starting server...\n");

    signal(SIGPIPE, SIG_IGN);

    lixs::os_linux::epoll epoll;
    lixs::event_mgr emgr(epoll);
    lixs::mstore::store store;
    lixs::xenstore xs(store, emgr, epoll);

    lixs::domain_mgr dmgr(xs, emgr, epoll);

    lixs::unix_sock_server nix(xs, dmgr, emgr, epoll,
            conf.unix_socket_path, conf.unix_socket_ro_path);

    lixs::xenbus* xenbus = NULL;
    lixs::virq_handler* dom_exc = NULL;

    if (conf.xenbus) {
        xenbus = new lixs::xenbus(xs, dmgr, emgr, epoll);
    }

    if (conf.virq_dom_exc) {
        dom_exc = new lixs::virq_handler(xs, dmgr, epoll);
    }


    emgr.enable();
    emgr_ptr = &emgr;
    signal(SIGINT, signal_handler);

    printf("[LiXS]: Entering main loop...\n");

    emgr.run();

    if (xenbus) {
        delete xenbus;
    }

    if (dom_exc) {
        delete dom_exc;
    }

    printf("[LiXS]: Server stoped!\n");

    return 0;
}

