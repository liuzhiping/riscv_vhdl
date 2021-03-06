/**
 * @file
 * @copyright  Copyright 2016 GNSS Sensor Ltd. All right reserved.
 * @author     Sergey Khabarov - sergeykhbr@gmail.com
 * @brief      elf-file loader class implementation.
 */

#include "console.h"
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "api_types.h"
#include "coreservices/iserial.h"
#include "coreservices/isignal.h"

namespace debugger {

/** Class registration in the Core */
REGISTER_CLASS(ConsoleService)

static const int STDIN = 0;

ConsoleService::ConsoleService(const char *name) 
    : IService(name), IHap(HAP_ConfigDone) {
    registerInterface(static_cast<IThread *>(this));
    registerInterface(static_cast<IConsole *>(this));
    registerInterface(static_cast<IHap *>(this));
    registerInterface(static_cast<IRawListener *>(this));
    registerAttribute("Enable", &isEnable_);
    registerAttribute("Consumer", &consumer_);
    registerAttribute("LogFile", &logFile_);
    registerAttribute("StepQueue", &stepQueue_);
    registerAttribute("Signals", &signals_);
    registerAttribute("Serial", &serial_);

    RISCV_mutex_init(&mutexConsoleOutput_);
    RISCV_event_create(&config_done_, "config_done");
    RISCV_register_hap(static_cast<IHap *>(this));

    isEnable_.make_boolean(true);
    consumer_.make_string("");
    logFile_.make_string("");
	stepQueue_.make_string("");
	signals_.make_string("");
    serial_.make_string("");

    logfile_ = NULL;
    iclk_ = NULL;
#ifdef DBG_ZEPHYR
    tst_cnt_ = 0;
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#else
    struct termios new_settings;
    tcgetattr(0, &original_settings_);
    new_settings = original_settings_;
     
    /// Disable canonical mode, and set buffer size to 1 byte
    new_settings.c_lflag &= ~(ICANON | ECHO);
    new_settings.c_cc[VTIME] = 0;
    new_settings.c_cc[VMIN] = 0;
     
    tcsetattr(STDIN, TCSANOW, &new_settings);
    term_fd_ = fileno(stdin);
#endif
}

ConsoleService::~ConsoleService() {
    if (logfile_) {
        fclose(logfile_);
    }
#if defined(_WIN32) || defined(__CYGWIN__)
#else
    tcsetattr(STDIN, TCSANOW, &original_settings_);
#endif
    RISCV_event_close(&config_done_);
    RISCV_mutex_destroy(&mutexConsoleOutput_);
}

void ConsoleService::postinitService() {
    ISerial *uart = static_cast<ISerial *>
            (RISCV_get_service_iface(serial_.to_string(), IFACE_SERIAL));
    if (uart) {
        uart->registerRawListener(static_cast<IRawListener *>(this));
    }

    iconsumer_ = static_cast<IKeyListener *>
            (RISCV_get_service_iface(consumer_.to_string(),
                                    IFACE_KEY_LISTENER));
    if (isEnable_.to_bool()) {
        if (!run()) {
            RISCV_error("Can't create thread.", NULL);
            return;
        }
    }
    if (logFile_.size()) {
        enableLogFile(logFile_.to_string());
    }

    iclk_ = static_cast<IClock *>
	    (RISCV_get_service_iface(stepQueue_.to_string(), IFACE_CLOCK));

    ISignal *itmp = static_cast<ISignal *>
        (RISCV_get_service_iface(signals_.to_string(), IFACE_SIGNAL));
    if (itmp) {
        itmp->registerSignalListener(static_cast<ISignalListener *>(this));
    }

#ifdef DBG_ZEPHYR
    if (iclk_) {
	    iclk_->registerStepCallback(static_cast<IClockListener *>(this), 550000);
	    iclk_->registerStepCallback(static_cast<IClockListener *>(this), 12000000);
	    iclk_->registerStepCallback(static_cast<IClockListener *>(this), 20000000);//6000000);
	    iclk_->registerStepCallback(static_cast<IClockListener *>(this), 35000000);
	}
#endif

    // Redirect output stream to a this console
    RISCV_set_default_output(static_cast<IConsole *>(this));
    std::cout << "riscv# " << cmdLine_.c_str();
    std::cout.flush();
}

void ConsoleService::predeleteService() {
    stop();
}

void ConsoleService::stepCallback(uint64_t t) {
#ifdef DBG_ZEPHYR
    if (iclk_ == NULL) {
        return;
    }
    IService *uart = static_cast<IService *>(RISCV_get_service("uart0"));
    if (uart) {
        ISerial *iserial = static_cast<ISerial *>(
                    uart->getInterface(IFACE_SERIAL));
        switch (tst_cnt_) {
        case 0:
            //iserial->writeData("ping", 4);
            iserial->writeData("dhry", 4);
            break;
        case 1:
            iserial->writeData("ticks", 5);
            break;
        case 2:
            iserial->writeData("help", 4);
            break;
        case 3:
            iserial->writeData("pnp", 4);
            break;
        default:;
        }
        tst_cnt_++;
    }
#endif
}



void ConsoleService::hapTriggered(EHapType type) {
    RISCV_event_set(&config_done_);
}

void ConsoleService::updateSignal(int start, int width, uint64_t value) {
    char sx[128];
    RISCV_sprintf(sx, sizeof(sx), "<led[%d:%d]> %02" RV_PRI64 "xh\n", 
                    start + width - 1, start, value);
    writeBuffer(sx);
}

void ConsoleService::busyLoop() {
    RISCV_event_wait(&config_done_);
    enum EScriptState {SCRIPT_normal, SCRIPT_comment, SCRIPT_command} scr_state;
    scr_state = SCRIPT_normal;
    std::string strcmd;

    const AttributeType *glb = RISCV_get_global_settings();
    if ((*glb)["ScriptFile"].size() > 0) {
        const char *script_name = (*glb)["ScriptFile"].to_string();
        FILE *script = fopen(script_name, "r");
        if (!script) {
            RISCV_error("Script file '%s' not found", script_name);
        } else if (iconsumer_) {
            bool crlf = false;
            char symb[2];
            while (!feof(script)) {
                fread(symb, 1, 1, script);

                switch (scr_state) {
                case SCRIPT_normal:
                    if (crlf && symb[0] == '\n') {
                        crlf = false;
                    } else if (symb[0] == '/') {
                        fread(&symb[1], 1, 1, script);
                        if (symb[1] == '/') {
                            scr_state = SCRIPT_comment;
                        } else {
                            iconsumer_->keyUp(symb[0]);
                            fseek(script, -1, SEEK_CUR);
                        }
                    } else if (symb[0] == '-') {
                        scr_state = SCRIPT_command;
                        strcmd = "";
                    } else {
                        iconsumer_->keyUp(symb[0]);
                    }
                    break;
                case SCRIPT_command:
                    if (symb[0] == '\r' || symb[0] == '\n') {
                        scr_state = SCRIPT_normal;
                        processScriptCommand(strcmd.c_str());
                    } else {
                        strcmd += symb[0];
                    }
                    break;
                case SCRIPT_comment:
                    if (symb[0] == '\r' || symb[0] == '\n') {
                        scr_state = SCRIPT_normal;
                    }
                default:;
                }

                crlf = symb[0] == '\r';
            }
            RISCV_info("Script '%s' was finished", script_name);
        }
    }


    while (isEnabled()) {
        if (isData()) {
            iconsumer_->keyUp(getData());
        }
        RISCV_sleep_ms(50);
    }
    loopEnable_ = false;
    threadInit_.Handle = 0;
}

void ConsoleService::processScriptCommand(const char *cmd) {
    AttributeType t1;
    t1.from_config(cmd);
    if (!t1.is_list()) {
        return;
    }
#if!defined(DBG_ZEPHYR)
    if (strcmp(t1[0u].to_string(), "wait") == 0) {
        RISCV_sleep_ms(static_cast<int>(t1[1].to_int64()));
    } else if (strcmp(t1[0u].to_string(), "uart0") == 0) {
        IService *uart = static_cast<IService *>(RISCV_get_service("uart0"));
        if (uart) {
            ISerial *iserial = static_cast<ISerial *>(
                        uart->getInterface(IFACE_SERIAL));
            iserial->writeData(t1[1].to_string(), t1[1].size());
        }
    }
#endif
}

void ConsoleService::updateData(const char *buf, int buflen) {
    for (int i = 0; i < buflen; i++) {
        if (buf[i] == '\r' || buf[i] == '\n') {
            if (serial_input_.size()) {
                serial_input_ = "<serialconsole> " + serial_input_ + "\n";
                writeBuffer(serial_input_.c_str());
            }
            serial_input_ .clear();
        } else {
            serial_input_ += buf[i];
        }
    }
}

void ConsoleService::writeBuffer(const char *buf) {
    RISCV_mutex_lock(&mutexConsoleOutput_);
    clearLine();
    std::cout << buf;
    std::cout << "riscv# " << cmdLine_.c_str();
    std::cout.flush();

    if (logfile_) {
        fwrite(buf, strlen(buf), 1, logfile_);
        fflush(logfile_);
    }
    RISCV_mutex_unlock(&mutexConsoleOutput_);
}

void ConsoleService::writeCommand(const char *cmd) {
    RISCV_mutex_lock(&mutexConsoleOutput_);
    clearLine();
    std::cout << "riscv# " << cmd << "\r\n";
    if (logfile_) {
        int len = sprintf(tmpbuf_, "riscv# %s\n", cmd);
        fwrite(tmpbuf_, len, 1, logfile_);
        fflush(logfile_);
    }
    RISCV_mutex_unlock(&mutexConsoleOutput_);
}

void ConsoleService::setCmdString(const char *buf) {
    RISCV_mutex_lock(&mutexConsoleOutput_);
    if (strlen(buf) < cmdLine_.size()) {
        clearLine();
    } else {
        std::cout << "\r";
    }
    cmdLine_ = std::string(buf);

    std::cout << "riscv# " << cmdLine_.c_str();
    std::cout.flush();
    RISCV_mutex_unlock(&mutexConsoleOutput_);
}

void ConsoleService::clearLine() {
    std::cout << "\r                                                      \r";
}

int ConsoleService::registerKeyListener(IFace *iface) {
    if (strcmp(iface->getFaceName(), IFACE_KEY_LISTENER)) {
        RISCV_error("Wrong interface '%s'", iface->getFaceName());
        return -1;
    }
    AttributeType t1(iface);
    keyListeners_.add_to_list(&t1);
    return 0;
}

void ConsoleService::enableLogFile(const char *filename) {
    if (logfile_) {
        fclose(logfile_);
        logfile_ = NULL;
    }
    logfile_ = fopen(filename, "w");
    if (!logfile_) {
        RISCV_error("Can not open file '%s'", filename);
    }
}

bool ConsoleService::isData() {
#if defined(_WIN32) || defined(__CYGWIN__)
    return _kbhit() ? true: false;
#else
    int bytesWaiting;
    ioctl(STDIN, FIONREAD, &bytesWaiting);
    return bytesWaiting != 0;
#endif
}

int ConsoleService::getData() {
#if defined(_WIN32) || defined(__CYGWIN__)
    return _getch();
#else
   unsigned char ch;
   //int err = 
   read(term_fd_, &ch, sizeof(ch));
   return ch;
    //return getchar();
#endif
}

}  // namespace debugger
