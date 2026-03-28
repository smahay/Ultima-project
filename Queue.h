//This is Queue.h

#ifndef QUEUE_H
#define QUEUE_H

#include <iostream>
using namespace std;

template <class T>
class Queue
{
private:
    struct Node
    {
        T data;
        Node* next;
    };

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
        if (isEmpty())
        {
            cout << "Queue is empty!" << endl;
            exit(1);
        }

        Node* temp = frontPtr;
        T value = frontPtr->data;
        frontPtr = frontPtr->next;

        if (frontPtr == nullptr)
        {
            rearPtr = nullptr;
        }

        delete temp;
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
    std::string to_string() const
    {
        std::string result;
        Node* current = frontPtr;

        while (current != nullptr)
        {
            result += std::to_string(current->data) + " ";
            current = current->next;
        }

        if (result.empty())
        {
            result = "EMPTY";
        }

        return result;
    }
};

#endif