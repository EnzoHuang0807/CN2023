

# CN HW1

### 1.

1. Go to `Statistics` -> `I/O Graphs`
   After adding a filter to remove TCP error packets, the transmit rate limit is 260 KBps $\approx$ 2080 Kbps
   ![](https://hackmd.io/_uploads/ry-J3mYgp.png)

2. `140.112.28.151`
   Observe the packets using TCP and DNS protocol. We can find that `140.112.28.151` is an IP address that is frequently seen as either source or destination.
   
3. [Reference](https://www.geeksforgeeks.org/differences-between-tcp-and-udp/)

   Apply filter `ip.src == 140.112.28.151 && tcp.srcport == 5555` and follow the stream to get the secret message sent via TCP
   Apply filter `ip.dst == 140.112.28.151 && udp.dstport == 48763` and follow the stream to get the secret message sent via UDP
   * secret message via TCP : 418 I'm a teapot
   * secret message via UDP : Baby shark, doo doo doo doo doo doo
   
   The main difference between TCP and UDP is that TCP is a connection-oriented protocol, meaning that client and server should establish a connection before transmitting data and should close the connection after transmitting the data. Therefore, more packets (overheads) are needed to transfer data using TCP. On the otherhand, UDP does not require connection establishment and is able to transfer data directly.
   
4. [Reference](https://chat.openai.com/share/d7ef132d-4c39-45dd-b08a-53d631feb983)

   * Network Layer Header
   
     1. Check the IP Version field: In an IPv4 header, the IP version field has a value of '4' (or '0100' in binary), whereas in an IPv6 header, the IP version field has a value of '6' (or '0110' in binary).
     2. IPv4 header has a Protocol field that indicates the protocol used by the upper-layer, while IPv6 header has a Next Header field that specifies the type of the next header, including both extension headers and upper-layer protocols. 

     * IPv4 Header
   
       ![](https://hackmd.io/_uploads/BJZHfeDx6.png)
     * IPv6 Header
   
       ![](https://hackmd.io/_uploads/rJ4wMlwgT.png)
       
   * Link Layer Header
     1. `Type` field indicates the IP version that is used for the packet
     * IPv4
     
       ![](https://hackmd.io/_uploads/HyvYu7Kep.png)
       
     * IPv6

       ![](https://hackmd.io/_uploads/BJ83umtx6.png)

5. Apply filter `dns contains wikipedia` and locate the packet that contains 

   ```
   Standard query response 0x50e7 A zh.wikipedia.org
   ```
   1. The packet contains DNS query to find the IPv4 address of `zh.wikipedia.org` and the response sent from DNS server. 
   2.  
       * type A : IPv4 address that a domain name corresponds to
       * type CNAME : Canonical name functions as an alias for a machine.
       * type NS : Name servers. Servers that store the correspondence between domain name and IP address 
   3. According to the response, `zh.wikipedia.org` has the same IPv4 address as `dyna.wikimedia.org`. Therefore, the IP address of `zh.wikipedia.org` is  103.102.166.224
   
   ![](https://hackmd.io/_uploads/rkyUNgvxT.png)


### 2.

1. Follow the TCP stream and get the result : 
   * UserName : cnta
   * Password : ji32k7au4a83
   
   ![](https://hackmd.io/_uploads/SkbKRiUe6.png)
   
2. 
   Based on the TCP stream found in problem 1, we can observe that packets containing requets to the server have a common destination port. 
  
   Based on the TCP stream found in problem 1, mark the packet that contains `STOR lorem.txt`, `STOR link.txt`, `STOR midterm.txt` , and `STOR puppy.png`. Packets containing file contents can be found near them. We can find the ports on the server that we send file contents with according to the destination ports of those packets. 
   
    * Server is listening to port 5000 for FTP requests
   * The ports on the server we send file contents with are :
     
     | FileName    | Port      |
     | --------    | --------  | 
     | lorem.txt   | 30012     |
     | links.txt   | 30013     |
     | midterm.txt | 30010     |
     | puppy.png   | 30016     |
 
3. Based on the TCP stream found in problem 1, mark the packet that contains `STOR midterm.txt`. A packet with length of 3127 bytes can be found near the packet. 
   Follow the packet and found the content of `midterm.txt`
   * FileName : midterm.txt
   * Number of Questions : 10
   ![](https://hackmd.io/_uploads/HJ2eM3Lgp.png)

### 3.

![](https://hackmd.io/_uploads/HJj1jhLlp.png)

1. [Reference](https://www.cloudflare.com/zh-tw/learning/email-security/smtp-port-25-587/)

    According to the TCP stream, ESMTP protocol is used.

     * Server Port : 4000
     * Application Protocol : MIME
     * Port that the server typically uses : 25
     
2. Follow the TCP stream 
     * Sender : prof.devil@notearuniv.edu
     * Receiver : wanna.cry@notearuniv.edu
     * Subject : Signing up for the course
3. Yes 
     * Course : Assembly Languages from Beginner to Quitter.
     * Permission Code : toRt0R-53d-@CcuM54n-bibenduM

4. No, the client is using TCP. The disadvantage without using TLS is that the transferring data are not encrypted. Therefore, it can be seen by others under the same internet.

### 4.

1. Find the packet that uses `HTTP` protocol to send a POST request to `140.112.28.111`
   Username and password can be found in the content of the packet.
   ![](https://hackmd.io/_uploads/Hy6svqLx6.png)

2. [Reference](https://www.deptagency.com/insight/the-dangers-of-non-secure-http/)

   We cannot find any packet that contains the password; instead, we can only find a packet containing encrypted data using `TLS` protocol.
   
   The result is caused by the use of `HTTPS` protocol. Data in packets are encrypted if `HTTPS` is used.
   ![](https://hackmd.io/_uploads/SkTphc8ga.png)
   
   
### 5.

1. The web server sends a POST request to `http://voip.csie.org:4071/submit` when we press the send button 
   * Command : 
   ```
   curl -i -X POST http://voip.csie.org:4071/submit\ 
   -H "Content-Type: application/x-www-form-urlencoded" \
   -d "username=B10902068&password=51"
   ```
   ![](https://hackmd.io/_uploads/BkExfsUxp.png)
   ![](https://hackmd.io/_uploads/Hk8m-i8xT.png)
   
2. 
   * Command : 
   ```
   curl -i -X POST http://voip.csie.org:4071/submit \
   -H "Content-Type: application/x-www-form-urlencoded" \
   -d "username=B10902068&password=51&secret=CN"
   ```
   * Response : 
   ```
   Hello B10902068. Your secret is "THE_dUe_Da7e_0F_@Ss19NmeNT_1_Is_Oct_4Th"
   ```
   ![](https://hackmd.io/_uploads/SJdGfjLx6.png)

### 6. 

1. [Reference](https://www.fortinet.com/resources/cyberglossary/traceroutes)
   The traceroute command works by sending ICMP packets to routers that would be reached on the path towards destination. The routers would then return the packets to sender. After receiving the returned packets, the command would output the path to `8.8.8.8` and the time delay of each routers.
   ![](https://hackmd.io/_uploads/Syt-9iUxT.png)

2.  [Reference](https://www.baeldung.com/linux/traceroute-three-stars)
    
    Instead of providing the information of the next router, only three star characters are presented in each hop after the fourth one.
    
    The reason that cause `traceroute` to behave like this is that no router beyond `140.112.xxx.xxx` has responded. A sequence of three-star hops indicates that the routers don’t respond because their owners forbade them to.

    ![](https://hackmd.io/_uploads/r1uEiiIga.png)

### 7. 

1. The IP address is `140.112.30.26`
   ![](https://hackmd.io/_uploads/HyaWNj8la.png)
   
2. [Reference](https://www.cloudflare.com/zh-tw/learning/dns/glossary/round-robin-dns/)
   Two of the IPs are `205.251.242.103`, `54.239.28.85`and `52.94.236.248`
   Using several different IP addresses for a single domain name is a technique to achieve load balancing.
   ![](https://hackmd.io/_uploads/ByVBEo8e6.png)



