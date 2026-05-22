#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#include "tcgi.h"

#define RADIUS_SERVER "127.0.0.1"
#define CLIENT_SECRET "testing123"

#define MAXLEN 4096
#define CNTPER10MIN 30

void check_rate(void)
{
	char fname[200];
	time_t t;
	struct tm *ctm;
	time(&t);
	ctm = localtime(&t);
	sprintf(fname, "/dev/shm/%04d.%02d.%02d.%02d%02d",
		ctm->tm_year + 1900, ctm->tm_mon + 1, ctm->tm_mday, ctm->tm_hour,
		ctm->tm_min / 10 * 10);
	FILE *fp;
	fp = fopen(fname, "r+");
	if (fp == NULL) {
		fp = fopen(fname, "w");
		fprintf(fp, "1");
		fclose(fp);
		return;
	}

	char buf[100];
	int n;
	buf[0] = 0;
	fgets(buf, 10, fp);
	n = atoi(buf);
	if (n > CNTPER10MIN) {
		fclose(fp);
		sleep(4);
		printf("每10分钟允许%d个请求，请稍后再来测试", CNTPER10MIN);
		exit(0);
	}
	rewind(fp);
	fprintf(fp, "%d", n + 1);
	fclose(fp);
}

void check_login(char *login)
{
	char *allow_str = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ@.-_";
	char *p;
	p = login;
	while (*p) {
		if (strchr(allow_str, *p) == 0) {
			printf("illegal character %c found in login name", *p);
			exit(0);
		}
		p++;
	}
}

void check_pass(char *pass)
{
	char *allow_str =
	    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ@.~!@#$%^&*()-_+={}[];:',.?/\\";
	char *p;
	p = pass;
	while (*p) {
		if (strchr(allow_str, *p) == 0) {
			printf("illegal character %c found in password", *p);
			exit(0);
		}
		p++;
	}
}

int main(int argc, char *argv[])
{
	dict *cgid;
	FILE *fp;
	int ret;
	char *login, *pass, filename[MAXLEN], filename_out[MAXLEN], buf[MAXLEN], *res;

	cgid = tcgi_parse();

	login = dict_get(cgid, "login", NULL);
	pass = dict_get(cgid, "password", NULL);

	printf("%s", "Content-Type: text/html\n\n");
	printf("%s", "<html><head><title>eduroam test</title></head><body>");

	check_rate();

	if ((login == NULL) || (pass == NULL)) {
		printf("%s", "<h2>eduroam test</h2>"
		       "<p>Provide just <b>TEST</b> credentials, do not entry credentials of real accounts.</p>"
		       "<p>Test tries EAP-PEAP MSCHAPv2 and EAP-TTLS PAP authentication.</p>"
		       "<p>No results and login/passwords are stored.</p>"
		       "<form action=/cgi-bin/eduroam-test.cgi method=GET>"
		       "Login: <input type=text name=login><br>"
		       "Password: <input type=text name=password><br>"
		       "<input type=submit value=Submit>"
		       "</form>"
		       "<p>Supported by CHAIN-REDS project and CESNET</p>" "</body></html>");
		exit(0);
	}

	check_login(login);
	check_pass(pass);

	printf("%s", "<a href=#mschapv2>EAP-PEAP MSCHAPv2 results</a><br>");
	printf("%s", "<a href=#pap>EAP-TTLS PAP results</a><br>");

	// step 1: EAP-PEAR MSCHAPv2 test
	printf("<a id=mschapv2><h2>Testing EAP-PEAP MSCHAPv2</h2></a>");

	// step 1.1: create and write config file
	snprintf(filename, sizeof(filename), "/dev/shm/radcfg.XXXXXX");
	{
		int fd = mkstemp(filename);
		if (fd == -1) {
			printf("create config file error");
			exit(0);
		}
		fchmod(fd, 0600);
		fp = fdopen(fd, "w");
	}
	if (fp == NULL) {
		printf("fdopen file %s error", filename);
		exit(0);
	}
	fprintf(fp, "network={\n"
		"ssid=\"eduroam\"\n"
		"key_mgmt=WPA-EAP\n"
		"eap=PEAP\n"
		"identity=\"%s\"\n"
		"anonymous_identity=\"%s\"\n"
		"password=\"%s\"\n" "phase2=\"autheap=MSCHAPV2\"\n" "}", login, login, pass);

	fclose(fp);

	// step 1.2: show config file
	printf("%s", "<h3>Configuration file used</h3>");
	printf("<pre>\n");
	fp = fopen(filename, "r");
	if (fp == NULL) {
		printf("open file %s error", filename);
		exit(0);
	}
	while (fgets(buf, MAXLEN, fp))
		printf("%s", buf);
	fclose(fp);
	printf("%s", "</pre>\n");

	// step 1.3: create output file and do the test
	snprintf(filename_out, sizeof(filename_out), "/dev/shm/radtest.XXXXXX");
	{
		int fd = mkstemp(filename_out);
		if (fd == -1) {
			printf("create output file error");
			exit(0);
		}
		fchmod(fd, 0600);
		close(fd);
	}
	snprintf(buf, sizeof(buf), "/usr/local/bin/eapol_test -c %s -s %s -a %s 2>&1 > %s",
		filename, CLIENT_SECRET, RADIUS_SERVER, filename_out);
	ret = system(buf);
	if (ret != 0)
		res = "<span style=\"color: red;\">FAILURE</span>";
	else
		res = "<span style=\"color: green;\">OK</span>";

	// step 1.4: show the result
	printf("<h3>Results of the test: %s</h3>", res);
	printf("%s", "<pre>\n");
	fp = fopen(filename_out, "r");
	if (fp == NULL) {
		printf("open file %s error", filename_out);
		exit(0);
	}
	while (fgets(buf, MAXLEN, fp))
		printf("%s", buf);
	fclose(fp);
	printf("%s", "</pre>\n");
	unlink(filename);
	unlink(filename_out);

	// step 2: EAP-TLS PAP test
	printf("<a id=pap><h2>Testing EAP-TLS PAP</h2></a>");

	// step 2.1: create and write config file
	snprintf(filename, sizeof(filename), "/dev/shm/radcfg.XXXXXX");
	{
		int fd = mkstemp(filename);
		if (fd == -1) {
			printf("create config file error");
			exit(0);
		}
		fchmod(fd, 0600);
		fp = fdopen(fd, "w");
	}
	if (fp == NULL) {
		printf("fdopen file %s error", filename);
		exit(0);
	}
	fprintf(fp, "network={\n"
		"ssid=\"eduroam\"\n"
		"key_mgmt=WPA-EAP\n"
		"eap=TTLS\n"
		"identity=\"%s\"\n"
		"anonymous_identity=\"%s\"\n"
		"password=\"%s\"\n" "phase2=\"auth=PAP\"\n" "}", login, login, pass);

	fclose(fp);

	// step 2.2: show config file
	printf("%s", "<h3>Configuration file used</h3>");

	printf("<pre>\n");
	fp = fopen(filename, "r");
	if (fp == NULL) {
		printf("open file %s error", filename);
		exit(0);
	}

	while (fgets(buf, MAXLEN, fp)) {
		printf("%s", buf);
	}
	fclose(fp);
	printf("%s", "</pre>\n");

	// step 2.3: create output file and do the test
	snprintf(filename_out, sizeof(filename_out), "/dev/shm/radtest.XXXXXX");
	{
		int fd = mkstemp(filename_out);
		if (fd == -1) {
			printf("create output file error");
			exit(0);
		}
		fchmod(fd, 0600);
		close(fd);
	}
	snprintf(buf, sizeof(buf), "/usr/local/bin/eapol_test -c %s -s %s -a %s 2>&1 > %s",
		filename, CLIENT_SECRET, RADIUS_SERVER, filename_out);

	ret = system(buf);
	if (ret != 0)
		res = "<span style=\"color: red;\">FAILURE</span>";
	else
		res = "<span style=\"color: green;\">OK</span>";

	// step 2.4: show the result
	printf("<h3>Results of the test: %s</h3>", res);
	printf("%s", "<pre>\n");
	fp = fopen(filename_out, "r");
	if (fp == NULL) {
		printf("open file %s error", filename_out);
		exit(0);
	}
	while (fgets(buf, MAXLEN, fp))
		printf("%s", buf);
	fclose(fp);
	printf("%s", "</pre>\n");
	unlink(filename);
	unlink(filename_out);

	printf("%s", "</body></html>");
	exit(0);
}
