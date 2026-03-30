#ifndef QUEUE_H
#define QUEUE_H

#include <iostream>
#include <string>
using namespace std;

template <class T>
class Queue
{
private:
    // Queue node.
    struct Node
    {
        T data;
        Node* next;
    };

    // Front/rear pointers.
    Node* frontPtr;
    Node* rearPtr;

public:
    Queue()
    {
        frontPtr = nullptr;
        rearPtr = nullptr;
    }

    ~Queue()
    {
        while (!isEmpty())
        {
            De_Q();
        }
    }

    void En_Q(T value)
    {
        // Enqueue at rear.
        Node* newNode = new Node{value, nullptr};

        if (rearPtr == nullptr)
        {
            frontPtr = rearPtr = newNode;
        }
        else
        {
            rearPtr->next = newNode;
            rearPtr = newNode;
        }
    }

    T De_Q()
    {
        // Dequeue from front.
        if (isEmpty())
        {
            cout << "Queue is empty!" << endl;
            exit(1);
        }

        T value = frontPtr->data;
        frontPtr = frontPtr->next;

        if (frontPtr == nullptr)
        {
            rearPtr = nullptr;
        }

        return value;
    }

    bool isEmpty()
    {
        return frontPtr == nullptr;
    }

    void Print()
    {
        Node* current = frontPtr;
        while (current != nullptr)
        {
            cout << current->data << " ";
            current = current->next;
        }
        cout << endl;
    }

    std::string to_string()
    {
        // For log output.
        std::string out;
        Node* current = frontPtr;

        while (current != nullptr)
        {
            out += std::to_string(current->data);
            if (current->next != nullptr)
            {
                out += "-->";
            }
            current = current->next;
        }

        if (out.empty())
        {
            out = "EMPTY";
        }

        return out;
    }
};

#endif
