/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-  1 byte  ->|<-             the rest            ->|
 *       | payload size |<-             payload             ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdt_struct.h"
#include "rdt_sender.h"

/* helper constants and functions */
#include <deque>
#include <vector>

const int WINDOW_SIZE = 10;
const int MAX_SEQ = 25;
const double TIMEOUT = 0.3;

static struct SenderWindow {
    int begin;
    int end;
    int size;
    std::vector<bool> ack_record;

    SenderWindow():begin(0), end(WINDOW_SIZE - 1), size(0) {}

    void slideForward(int steps) {
        begin = (begin + steps) % MAX_SEQ;
        end = (end + steps) % MAX_SEQ;
    }

    bool isFull() {
        return (size == WINDOW_SIZE);
    }

    bool isInRange(int seqnum) {
        if (begin < end) {
            return ((seqnum >= begin) && (seqnum <= end));
        } else {
            return (((seqnum >= begin) && (seqnum < MAX_SEQ)) || 
                    ((seqnum >= 0) && (seqnum <= end)));
        }
    }

    void debug() {
        printf("===sender debug: window from %d to %d, size = %d\n", begin, end, size);
    }
} window;

static struct SenderBuffer {
    int seqnum;
    std::deque<packet> pkts;

    SenderBuffer():seqnum(0) {}

    void addSeqNum(int num) {
        seqnum = (seqnum + num) % MAX_SEQ;
    }

    void debug() {
        printf("===sender debug: seqnum = %d, pkts.size = %d\n", seqnum, pkts.size());
    }
} buffer;

//static Window window;
//static Buffer buffer;
static unsigned short checksum(const char *buf, unsigned size);

/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    window.begin = 0;
    window.end = WINDOW_SIZE - 1;
    window.size = 0;
    window.ack_record.assign(MAX_SEQ, false);
    buffer.seqnum = 0;
    //buffer.pkts.assign(MAX_SEQ, packet());
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
} 

/* sender finalization, called once at the very end.  
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some 
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a message is passed from the upper layer at the 
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
    //printf("enter Sender_FromUpperLayer\n");
    /* pkt_size = 1 | seqnum_size = 1 | acknum_size = 1 | checksum_size = 2 */
    
    /* 1-byte header indicating the size of the payload */
    int header_size = 5;

    /* maximum payload size */
    int maxpayload_size = RDT_PKTSIZE - header_size;
    /* split the message if it is too big */

    /* reuse the same packet data structure */
    packet pkt;

    /* the cursor always points to the first unsent byte in the message */
    int cursor = 0;

    while (msg->size-cursor > maxpayload_size) {
	    /* fill in the packet */
        memset(pkt.data, 0, RDT_PKTSIZE + 1);
	    pkt.data[0] = maxpayload_size;
        
        pkt.data[1] = (buffer.seqnum + buffer.pkts.size()) % MAX_SEQ;

        pkt.data[2] = 0xFF; // 0xFF represents invalid
        
        unsigned short *csp = (unsigned short *)(pkt.data + 3);
        *csp = checksum(msg->data+cursor, maxpayload_size);
        
	    memcpy(pkt.data+header_size, msg->data+cursor, maxpayload_size);
        
        /* push into buffer */
        buffer.pkts.push_back(pkt);
	    /* send it out through the lower layer */
        if (!window.isFull()) {
            if (window.size == 0) {
                Sender_StartTimer(TIMEOUT);
            }
            window.size++;
            //printf("send packet num = %d size = %d data = %s\n", pkt.data[1], pkt.data[0], &pkt.data[5]);
	        Sender_ToLowerLayer(&pkt);
            //window.debug();
            //buffer.debug();
        }

	    /* move the cursor */
	    cursor += maxpayload_size;
    }

    /* send out the last packet */
    if (msg->size > cursor) {
	    /* fill in the packet */
        memset(pkt.data, 0, RDT_PKTSIZE + 1);
	    pkt.data[0] = msg->size-cursor;
        
        pkt.data[1] = (buffer.seqnum + buffer.pkts.size()) % MAX_SEQ;
        
        pkt.data[2] = 0xFF; // 0xFF represents invalid
        
        unsigned short *csp = (unsigned short *)(pkt.data + 3);
        *csp = checksum(msg->data+cursor, msg->size - cursor);
        
	    memcpy(pkt.data+header_size, msg->data+cursor, pkt.data[0]);

        /* push into buffer */
        buffer.pkts.push_back(pkt);
	    /* send it out through the lower layer */
        if (!window.isFull()) {
            if (window.size == 0) {
                Sender_StartTimer(TIMEOUT);
            }
            window.size++;
            //printf("send packet num = %d size = %d data = %s\n", pkt.data[1], pkt.data[0], &pkt.data[5]);
	        Sender_ToLowerLayer(&pkt);
            //window.debug();
            //buffer.debug();
        }
    }
}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    //printf("enter Sender_FromLowerLayer\n");
    int header_size = 5;
    int pkt_size = pkt->data[0];
    //printf("===sender pkt_size = %d\n", pkt_size);
    if ((pkt_size < 0) || (pkt_size > (RDT_PKTSIZE - header_size))) {
        //printf("pkt_size = %d\n", pkt_size);
        return;
    }
    unsigned short verify = checksum(pkt->data+header_size, pkt_size);
    unsigned short checksum = *(unsigned short *)(pkt->data + 3);
    if (verify != checksum) {
        //printf("sender checksum = %u verify = %u\n", checksum, verify);
        return;
    }

    int acknum = pkt->data[2] & 0xFF;
    //printf("acknum = %d\n", acknum);
    if (!window.isInRange(acknum)) {
        return; // have acked before
    }
    window.ack_record[acknum] = true;
    bool resetTimer = false;
    while (window.ack_record[buffer.seqnum]) {
        window.ack_record[buffer.seqnum] = false;
        window.size--;
        buffer.pkts.pop_front();
        buffer.addSeqNum(1);
        resetTimer = true;
        window.slideForward(1);
            //window.debug();
            //buffer.debug();
    }

    /* if no more packets is remained */
    if (window.size == buffer.pkts.size()) {
        return;
    }

    int max_size = (WINDOW_SIZE > buffer.pkts.size()) ? buffer.pkts.size() : WINDOW_SIZE;
    while (window.size < max_size) {
        Sender_ToLowerLayer(&(buffer.pkts[window.size - 1]));
        window.size++;
            //window.debug();
            //buffer.debug();
    }
    if (resetTimer) {
        Sender_StartTimer(TIMEOUT);
    }
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    if (buffer.pkts.empty()) {
        return;
    }

    for (int i = 0; i < window.size; i++) {
        if (!window.ack_record[(buffer.seqnum + i) % MAX_SEQ]) {
            //printf("enter Sender_Timeout resend num = %d\n", (buffer.seqnum + i) % MAX_SEQ);
            Sender_ToLowerLayer(&(buffer.pkts[i]));
            Sender_StartTimer(TIMEOUT);
        }
    }
}

static unsigned short checksum(const char *buf, unsigned size) {
    if (!buf) {
        printf("warning!!! size = %u\n",size);
        return 0;
    }
    if (size) {
        size--;
    }

    unsigned long long sum = 0;
    const unsigned long long *b = (unsigned long long *) buf;

    unsigned t1, t2;
    unsigned short t3, t4;

    /* Main loop - 8 bytes at a time */
    while (size >= sizeof(unsigned long long)) {
        unsigned long long s = *b++;
        sum += s;
        if (sum < s) {
            sum++;
        }
        size -= 8;
    }

    /* Handle tail less than 8-bytes long */
    buf = (const char *) b;
    if (size & 4) {
        unsigned s = *(unsigned *)buf;
        sum += s;
        if (sum < s) {
            sum++;
        }
        buf += 4;
    }

    if (size & 2) {
        unsigned short s = *(unsigned short *) buf;
        sum += s;
        if (sum < s) {
            sum++;
        }
        buf += 2;
    }

    if (size) {
        unsigned char s = *(unsigned char *) buf;
        sum += s;
        if (sum < s) {
            sum++;
        }
    }

    /* Fold down to 16 bits */
    t1 = sum;
    t2 = sum >> 32;
    t1 += t2;
    if (t1 < t2) {
        t1++;
    }
    t3 = t1;
    t4 = t1 >> 16;
    t3 += t4;
    if (t3 < t4) {
        t3++;
    }

    return ~t3;
}
