#include "mbed.h"
#include <string>

#define CHANNEL_NONE 0
#define CHANNEL_USB 1
#define CHANNEL_BLUETOOTH 2

#define UNIVERSAL_MAX_BUFFER_SIZE 256

#define SOURCE_USB 1
#define SOURCE_BLUETOOTH 2

#define READY_TO_READ (1UL << 0)
#define READY_TO_WRITE (1UL << 10)

typedef struct {

    string message;

    unsigned char source;

} message_t;

unsigned char connection_channel;

Mutex serialio;

Queue<message_t, 5> recieve_queue;
MemoryPool<message_t, 5> recieve_memory_pool;

Queue<string, 5> USB_trancieve_queue;
MemoryPool<string, 5> USB_trancieve_memory_pool;

Queue<string, 5> Bluetooth_trancieve_queue;
MemoryPool<string, 5> Bluetooth_trancieve_memory_pool;

Thread command_processor_thread;

BufferedSerial Serial_USB(PA_2, PA_3, 115200);

EventFlags usb_usart_flags;

Thread USB_tranciever_thread;

Thread USB_reciever_thread;

BufferedSerial Serial_Bluetooth(PC_10, PC_11, 9600);

EventFlags bluetooth_usart_flags;

Thread Bluetooth_tranciever_thread;

Thread Bluetooth_reciever_thread;

void process_to_input32_queue(MemoryPool<message_t, 5> &mpool, Queue<message_t, 5> &queue, char recieve_buffer[UNIVERSAL_MAX_BUFFER_SIZE], string &out_str, string separator, unsigned char source)
{

    out_str += recieve_buffer;

    string token;
    int pos;

    message_t *message;

    while ((pos = out_str.find(separator)) != string::npos)
    {

        token = out_str.substr(0, pos);
        out_str.erase(0, pos + separator.length());

        message = mpool.try_alloc();
        if (message != nullptr)
        {

            message->message = token;
            message->source = source;
            queue.try_put(message);

        }

    }

}

void put_onto_string32_queue(MemoryPool<string, 5> &mpool, Queue<string, 5> &queue, string message)
{

    string *sender;

    sender = mpool.try_alloc();
    if (sender != nullptr)
    {

        *sender = message;
        queue.try_put(sender);

    }

}

void echo(string message, unsigned char target)
{

    if (target == SOURCE_USB)
    {

        put_onto_string32_queue(USB_trancieve_memory_pool, USB_trancieve_queue, message);

    }
    else if (target == SOURCE_BLUETOOTH)
    {

        put_onto_string32_queue(Bluetooth_trancieve_memory_pool, Bluetooth_trancieve_queue, message);

    }

}

void connect(unsigned char source)
{

    if (source == SOURCE_USB)
    {

        put_onto_string32_queue(USB_trancieve_memory_pool, USB_trancieve_queue, "connected!");
        connection_channel = CHANNEL_USB;

    }
    else if (source == SOURCE_BLUETOOTH)
    {

        put_onto_string32_queue(Bluetooth_trancieve_memory_pool, Bluetooth_trancieve_queue, "connected!");
        connection_channel = CHANNEL_BLUETOOTH;

    }

}

void disconnect(unsigned char target)
{

    if (target == SOURCE_USB)
    {

        put_onto_string32_queue(USB_trancieve_memory_pool, USB_trancieve_queue, "disconnected!");
        connection_channel = CHANNEL_NONE;

    }
    else if (target == SOURCE_BLUETOOTH)
    {

        put_onto_string32_queue(Bluetooth_trancieve_memory_pool, Bluetooth_trancieve_queue, "disconnected!");
        connection_channel = CHANNEL_NONE;

    }

}

void command_processor()
{

    message_t *getter;

    while (true)
    {

        recieve_queue.try_get_for(Kernel::wait_for_u32_forever, &getter);

        if (getter->message.find("connect") == 0)
                connect(getter->source);

        if (getter->message.find("stop") == 0)
            true == true;

        else if (getter->message.find("echo") == 0)
            echo(getter->message, connection_channel);

        else if (getter->message.find("disconnect") == 0)
            disconnect(connection_channel);

    }

}

void USB_trancieve()
{

    char trancieve_buffer[UNIVERSAL_MAX_BUFFER_SIZE];

    memset(trancieve_buffer, 0, UNIVERSAL_MAX_BUFFER_SIZE);

    string *getter;

    int len;

    while (true)
    {

        USB_trancieve_queue.try_get_for(Kernel::wait_for_u32_forever, &getter);

        serialio.lock();

        strcpy(trancieve_buffer, getter->c_str());
        len = getter->length();

        USB_trancieve_memory_pool.free(getter);

        Serial_USB.write(trancieve_buffer, len);

        memset(trancieve_buffer, 0, UNIVERSAL_MAX_BUFFER_SIZE);

        serialio.unlock();

    }

}

void USB_io_interrupt()
{

    if (Serial_USB.readable() > 0)
        usb_usart_flags.set(READY_TO_READ);

}

void USB_recieve()
{

    Serial_USB.sigio(&USB_io_interrupt);

    char recieve_buffer[UNIVERSAL_MAX_BUFFER_SIZE];

    memset(recieve_buffer, 0, UNIVERSAL_MAX_BUFFER_SIZE);

    string output_str("");

    while (true)
    {

        usb_usart_flags.wait_all(READY_TO_READ, osWaitForever, false);

        serialio.lock();

        Serial_USB.read(recieve_buffer, UNIVERSAL_MAX_BUFFER_SIZE);

        process_to_input32_queue(recieve_memory_pool, recieve_queue, recieve_buffer, output_str, "!", SOURCE_USB);
        memset(recieve_buffer, 0, UNIVERSAL_MAX_BUFFER_SIZE);

        if (Serial_USB.readable() == 0)
        {
            
            usb_usart_flags.clear(READY_TO_READ);
            serialio.unlock();

        }

    }

}

void Bluetooth_trancieve()
{

    char trancieve_buffer[UNIVERSAL_MAX_BUFFER_SIZE];

    memset(trancieve_buffer, 0, UNIVERSAL_MAX_BUFFER_SIZE);

    string *getter;

    int len;

    while (true)
    {

        Bluetooth_trancieve_queue.try_get_for(Kernel::wait_for_u32_forever, &getter);

        serialio.lock();

        sprintf(trancieve_buffer, "%s\n",getter->c_str());
        len = getter->length() + 1;

        Bluetooth_trancieve_memory_pool.free(getter);

        Serial_Bluetooth.write(trancieve_buffer, len);

        memset(trancieve_buffer, 0, UNIVERSAL_MAX_BUFFER_SIZE);

        serialio.unlock();

    }

}

void Bluetooth_io_interrupt()
{

    if (Serial_Bluetooth.readable() > 0)
        bluetooth_usart_flags.set(READY_TO_READ);

}

void Bluetooth_recieve()
{

    Serial_Bluetooth.sigio(&Bluetooth_io_interrupt);

    char recieve_buffer[UNIVERSAL_MAX_BUFFER_SIZE];

    memset(recieve_buffer, 0, UNIVERSAL_MAX_BUFFER_SIZE);

    string output_str("");

    while (true)
    {

        bluetooth_usart_flags.wait_all(READY_TO_READ, osWaitForever, false);

        serialio.lock();

        Serial_Bluetooth.read(recieve_buffer, UNIVERSAL_MAX_BUFFER_SIZE);

        process_to_input32_queue(recieve_memory_pool, recieve_queue, recieve_buffer, output_str, "!", SOURCE_BLUETOOTH);
        memset(recieve_buffer, 0, UNIVERSAL_MAX_BUFFER_SIZE);

        if (Serial_Bluetooth.readable() == 0)
        {
         
            bluetooth_usart_flags.clear(READY_TO_READ);
            serialio.unlock();
            
        }

    }

}

int main()
{

    connection_channel = CHANNEL_NONE;

    USB_reciever_thread.start(&USB_recieve);
    Bluetooth_reciever_thread.start(&Bluetooth_recieve);
    command_processor_thread.start(&command_processor);
    USB_tranciever_thread.start(&USB_trancieve);
    Bluetooth_tranciever_thread.start(&Bluetooth_trancieve);

}