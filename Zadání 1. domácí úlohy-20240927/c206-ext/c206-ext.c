/*
 *  Předmět: Algoritmy (IAL) - FIT VUT v Brně
 *  Rozšíření pro příklad c206.c (Dvousměrně vázaný lineární seznam)
 *  Vytvořil: Daniel Dolejška, září 2024
 */
/**
 * @file c206-ext.c
 * @author xludvir00
 * @brief Rozšíření pro příklad c206.c (Dvousměrně vázaný lineární seznam)
 * @date 2024-09-30
 * 
*/

#include "c206-ext.h"

bool error_flag;
bool solved;

/**
 * Tato metoda simuluje příjem síťových paketů s určenou úrovní priority.
 * Přijaté pakety jsou zařazeny do odpovídajících front dle jejich priorit.
 * "Fronty" jsou v tomto cvičení reprezentovány dvousměrně vázanými seznamy
 * - ty totiž umožňují snazší úpravy pro již zařazené položky.
 * 
 * Parametr `packetLists` obsahuje jednotlivé seznamy paketů (`QosPacketListPtr`).
 * Pokud fronta s odpovídající prioritou neexistuje, tato metoda ji alokuje
 * a inicializuje. Za jejich korektní uvolnení odpovídá volající.
 * 
 * V případě, že by po zařazení paketu do seznamu počet prvků v cílovém seznamu
 * překročil stanovený MAX_PACKET_COUNT, dojde nejdříve k promazání položek seznamu.
 * V takovémto případě bude každá druhá položka ze seznamu zahozena nehledě
 * na její vlastní prioritu ovšem v pořadí přijetí.
 * 
 * @param packetLists Ukazatel na inicializovanou strukturu dvousměrně vázaného seznamu
 * @param packet Ukazatel na strukturu přijatého paketu
 */
void receive_packet(DLList *packetLists, PacketPtr packet) {
    QosPacketListPtr qosPacketList = NULL;

    // Prohledáme seznam, jestli už existuje fronta pro danou prioritu
    DLL_First(packetLists);
    while (DLL_IsActive(packetLists)) {
        QosPacketListPtr currentList;
        DLL_GetValue(packetLists, (long *)&currentList);
        if (currentList->priority == packet->priority) {
            qosPacketList = currentList;
            break;
        }
        DLL_Next(packetLists);
    }

    // Pokud fronta pro danou prioritu neexistuje, vytvoříme ji
    if (qosPacketList == NULL) {
        qosPacketList = (QosPacketListPtr)malloc(sizeof(QosPacketList));
        if (qosPacketList == NULL) {
            //DLL_Error();
            return;
        }
        qosPacketList->priority = packet->priority;
        qosPacketList->list = (DLList *)malloc(sizeof(DLList));
        if (qosPacketList->list == NULL) {
            free(qosPacketList);
            //DLL_Error();
            return;
        }
        DLL_Init(qosPacketList->list);
        DLL_InsertLast(packetLists, (long)qosPacketList);
    }

    // Přidáme nový paket do fronty (na konec)
    DLL_InsertLast(qosPacketList->list, (long)packet);

    // Kontrola, zda fronta přesáhla MAX_PACKET_COUNT
    if (qosPacketList->list->currentLength > MAX_PACKET_COUNT) {
        int index = 0;
        DLL_First(qosPacketList->list);
        DLLElementPtr elementToRemove = NULL;

        // Procházíme seznam a mažeme každý druhý prvek
        while (DLL_IsActive(qosPacketList->list)) {
            if (index % 2 == 1) {
                elementToRemove = qosPacketList->list->activeElement;
                DLL_Next(qosPacketList->list); // Přesuneme aktivní prvek na další
                if (elementToRemove == qosPacketList->list->firstElement) {
                    DLL_DeleteFirst(qosPacketList->list); // Smažeme první prvek
                } else if (elementToRemove == qosPacketList->list->lastElement) {
                    DLL_DeleteLast(qosPacketList->list);  // Smažeme poslední prvek
                } else {
                    // Smažeme prvek uvnitř seznamu
                    elementToRemove->previousElement->nextElement = elementToRemove->nextElement;
                    elementToRemove->nextElement->previousElement = elementToRemove->previousElement;
                    free(elementToRemove);
                    qosPacketList->list->currentLength--;
                }
            } else {
                DLL_Next(qosPacketList->list); // Posuneme se na další prvek
            }
            index++;
        }
    }
}


/**
 * Tato metoda simuluje výběr síťových paketů k odeslání. Výběr respektuje
 * relativní priority paketů mezi sebou, kde pakety s nejvyšší prioritou
 * jsou vždy odeslány nejdříve. Odesílání dále respektuje pořadí, ve kterém
 * byly pakety přijaty metodou `receive_packet`.
 * 
 * Odeslané pakety jsou ze zdrojového seznamu při odeslání odstraněny.
 * 
 * Parametr `packetLists` obsahuje ukazatele na jednotlivé seznamy paketů (`QosPacketListPtr`).
 * Parametr `outputPacketList` obsahuje ukazatele na odeslané pakety (`PacketPtr`).
 * 
 * @param packetLists Ukazatel na inicializovanou strukturu dvousměrně vázaného seznamu
 * @param outputPacketList Ukazatel na seznam paketů k odeslání
 * @param maxPacketCount Maximální počet paketů k odeslání
 */
void send_packets(DLList *packetLists, DLList *outputPacketList, int maxPacketCount) {
    int sentPackets = 0;

    // Procházíme fronty podle priority (od nejvyšší k nejnižší)
    while (sentPackets < maxPacketCount) {
        QosPacketListPtr highestPriorityList = NULL;
        DLL_First(packetLists);

        // Najdeme frontu s nejvyšší prioritou, která obsahuje pakety
        while (DLL_IsActive(packetLists)) {
            QosPacketListPtr currentList;
            DLL_GetValue(packetLists, (long *)&currentList);

            if (highestPriorityList == NULL || currentList->priority > highestPriorityList->priority) {
                if (currentList->list->firstElement != NULL) {
                    highestPriorityList = currentList;
                }
            }

            DLL_Next(packetLists);
        }

        // Pokud nejsou žádné fronty s pakety, ukončíme odesílání
        if (highestPriorityList == NULL) {
            break;
        }

        // Odesíláme pakety z fronty s nejvyšší prioritou
        while (sentPackets < maxPacketCount && highestPriorityList->list->firstElement != NULL) {
            long packet;
            DLL_GetFirst(highestPriorityList->list, &packet);

            // Přidáme paket do výstupního seznamu
            DLL_InsertLast(outputPacketList, packet);

            // Odebereme paket z fronty
            DLL_DeleteFirst(highestPriorityList->list);

            sentPackets++;
        }
    }
}
