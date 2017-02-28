/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
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
#include "rdt_receiver.h"

/* helper constants and functions */
#include <vector>

const int WINDOW_SIZE = 10;
const int MAX_SEQ = 25;
const double TIMEOUT = 0.3;

static struct ReceiverWindow {
    int begin;
    int end;
    int size;

    ReceiverWindow():begin(0), end(WINDOW_SIZE - 1), size(0) {}

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
        printf("===receiver debug: window from %d to %d, size = %d\n", begin, end, size);
    }
} window;

static struct ReceiverBuffer {
    int seqnum;
    std::vector<message *> msgs;

    ReceiverBuffer():seqnum(0) {}

    void addSeqNum(int num) {
        seqnum = (seqnum + num) % MAX_SEQ;
    }
    
    void debug() {
        printf("===receiver debug: seqnum = %d, pkts.size = %d\n", seqnum, msgs.size());
    }
} buffer;

//static Window window;
//static Buffer buffer;
static unsigned short checksum(const char *buf, unsigned size);

/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    window.begin = 0;
    window.end = WINDOW_SIZE - 1;
    window.size = 0;
    buffer.seqnum = 0;
    buffer.msgs.assign(MAX_SEQ, NULL);
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some 
   memory you allocated in Receiver_init(). */
void Receiver_Final()
{
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a packet is passed from the lower layer at the 
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
    //printf("enter Receiver_FromLowerLayer\n");
    /* pkt_size = 1 | seqnum_size = 1 | acknum_size = 1 | checksum_size = 2 */
    
    /* 1-byte header indicating the size of the payload */
    int header_size = 5;
    int pkt_size = pkt->data[0];
    //printf("===receiver pkt_size = %d\n", pkt_size);
    if ((pkt_size < 0) || (pkt_size > (RDT_PKTSIZE - header_size))) {
        //printf("pkt_size = %d\n", pkt_size);
        return;
    }
    unsigned short verify = checksum(pkt->data+header_size, pkt_size);
    unsigned short checksum = *(unsigned short *)(pkt->data + 3);
    int seqnum = pkt->data[1] & 0xFF;
    if (verify != checksum) { 
        //printf("seqnum = %d checksum = %u verify = %u data = %s\n", seqnum, checksum, verify, &pkt->data[5]);
        return;
    }
    
    //int seqnum = pkt->data[1] & 0xFF;
    pkt->data[1] = 0xFF; // 0xFF represents invalid
    pkt->data[2] = seqnum & 0xFF;
    Receiver_ToLowerLayer(pkt);
    /* out of range packet also need ACK */
    if (!window.isInRange(seqnum)) {
        //printf("%d out of range return\n", seqnum);
        //window.debug();
        //buffer.debug();
        return;
    }
    /* have acked more than once */
    if (buffer.msgs[seqnum]) {
        //printf("has been acked return\n");
        return;
    }

    /* construct a message and deliver to the upper layer */
    struct message *msg = (struct message*) malloc(sizeof(struct message));
    ASSERT(msg!=NULL);

    msg->size = pkt->data[0];

    /* sanity check in case the packet is corrupted */
    if (msg->size<0) msg->size=0;
    if (msg->size>RDT_PKTSIZE-header_size) msg->size=RDT_PKTSIZE-header_size;

    msg->data = (char*) malloc(msg->size + 1);
    ASSERT(msg->data!=NULL);
    memset(msg->data, 0, msg->size + 1);
    memcpy(msg->data, pkt->data+header_size, msg->size);
    //printf("receive packet num = %d data = %s\n", seqnum, &pkt->data[5]);

    buffer.msgs[seqnum] = msg;
    //printf("receive packet num = %d size = %d data = %s\n", seqnum, msg->size, buffer.msgs[seqnum]->data);
    //printf("receive packet num = %d size = %d data = %s\n", seqnum, msg->size, buffer.msgs[seqnum]->data);
    while (buffer.msgs[buffer.seqnum]) {
        //printf("Receiver_ToUpperLayer num = %d size = %d data = %s\n", buffer.seqnum, buffer.msgs[buffer.seqnum]->size, buffer.msgs[buffer.seqnum]->data);
        Receiver_ToUpperLayer(buffer.msgs[buffer.seqnum]);
        if (buffer.msgs[buffer.seqnum] && buffer.msgs[buffer.seqnum]->data) {
            free(buffer.msgs[buffer.seqnum]->data);
        }
        if (buffer.msgs[buffer.seqnum]) {
            free(buffer.msgs[buffer.seqnum]);
        }
        buffer.msgs[buffer.seqnum] = NULL;
        buffer.addSeqNum(1);
        window.slideForward(1);
        //window.debug();
    }
}

static unsigned short checksum(const char *buf, unsigned size) {
    if (!buf) {
        printf("warning!!! size = %u\n", size);
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
