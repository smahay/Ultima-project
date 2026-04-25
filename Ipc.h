#ifndef IPC_H
#define IPC_H

#include <iostream>
#include <ctime>

using namespace std;

// IPC mailbox
class ipc
{
    public:
        // Message type info (0=TEXT, 1=SERVICE, 2=NOTIFICATION).
        struct Message_Type
        {
            int Message_Type_Id;
            char Message_Type_Description[64];
        };

        struct Message
        {
            int Source_Task_Id;
            int Destination_Task_Id;
            time_t Message_Arrival_Time;
            Message_Type Msg_Type;
            int Msg_Size;
            char Msg_Text[33];
        };

    private:
        int max_tasks;

    public:
        ipc(int max_tasks);

        // Sends a message struct to the given task mailbox.
        int Message_Send(Message *message);

        // Send one message with input arguments.
        int Message_Send(int S_Id, int D_Id, char *Mess, int Mess_Type);

        // Receives a message.
        int Message_Receive(int Task_Id, Message *message);

        // Receive one message with output argument.
        int Message_Receive(int Task_Id, char *Mess, int *Mess_Type);

        // Count messages in one mailbox.
        int Message_Count(int Task_Id);

        // Count messages in all mailboxes.
        int Message_Count();

        // Print one mailbox.
        void Message_Print(int Task_Id);

        // Delete all messages in one mailbox.
        int Message_DeleteAll(int Task_Id);

        // Print every mailbox.
        void ipc_Message_Dump();
};

#endif