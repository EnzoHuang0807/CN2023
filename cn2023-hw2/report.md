

### 1.

![[dash.png]]

When current connection provides a higher bandwith, `dash.mpd` would request for `init-stream1.m4s` and `init-stream2.m4s` , thereby sending chunks of video and audio with higher quality.

On the other hand, when current connection provides a lower bandwith, `dash.mpd` would request for `init-stream0.m4s` and `init-stream3.m4s` , thereby sending chunks of video and audio with higher quality. 
### 2. 

#### Theoretical

Since MP4 files are comparatively large, serving the entire file might lead to large time delay. However, if we serve DASH files that allows client to request for chunks of the video, the delay time can be reduced substantially.

#### Parctical

When serving MP4 videos, the browser would download the entire file (which is comparatively) lage in order to play the video. On the other hand, when serving DASH files, the browser is able to play the video in segments by sending requests to get multiple chunks (comparatively small in sizes) with different quality based on current bandwidth and user preference.
### 3. 

[Reference](https://developer.mozilla.org/en-US/docs/Web/HTTP/Authentication)
#### Implementation of HTTP authentication

If the operations indicated by a request requires authentication, the server would look for `Authenticate` header in the request and decode its corresponding value into a credential (`username:password` pair for the server we've implemented). It then scans through the `secret` file to check whether the credential is valid.

If the credential is invalid or `Authenticate` header does not exist, the server would send a 401 (Unauthorized) response, with the header `WWW-Authenticate` providing information on how to authorize.
#### Security

HTTP basic authentication is not secure enough since it only encodes the credentials instead of encrpyting them. Applying other authentication schemes or using HTTP basic authentication over a secure connection (HTTPS/TLS) helps improve security.

### Bonus

* I use git to push my code when some features of the assignment are completed (file upload/download, video streaming, client commands, etc.) or some bug is fixed.

* The benefit of Git is that it allows me to back up my assignment in a proper manner. If I should encounter any unsolvable bug in the current version of my code, I can simply search for previous commit of my code and restore it.

* Using GitHub Classroom to submit homework is better since no additional time is needed to arrange and zip our code

### Reference

##### Credit : 
B10902138 陳德維

[Parsing HTTP requests](https://codereview.stackexchange.com/questions/188384/http-request-parser-in-c)
[URL decoding](https://stackoverflow.com/questions/2673207/c-c-url-decode-library)
[URL encoding](https://gist.github.com/jesobreira/4ba48d1699b7527a4a514bfa1d70f61a)
[HTTP Basic Authentication](https://developer.mozilla.org/en-US/docs/Web/HTTP/Authentication)
[multipart/form-data](https://datatracker.ietf.org/doc/html/rfc7578?fbclid=IwAR0CeA_Wma_wQhltNHI8LWJzyr6xz9RWIaPbWi_CvYb_aOZ0s-_hB1HiBdw)
[memmem()](https://stackoverflow.com/questions/2188914/c-searching-for-a-string-in-a-file)
[Read Directory](https://stackoverflow.com/questions/4204666/how-to-list-files-in-a-directory-in-a-c-program)