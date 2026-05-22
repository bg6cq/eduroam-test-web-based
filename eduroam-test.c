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

	if (strcmp(dict_get(cgid, "REMOTE_ADDR", NULL), "159.226.26.3") != 0)
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
	printf("%s",
	       "正将您输入的信息发给radius服务器测试，测试结果汇总如下：<p>\n");
	printf("%s", "<table border cellpadding=2 cellspacing=0><tr><td>");
	printf("%s", "认证类型</td><td>测试结果</td></tr>\n");
	printf("%s", "<tr><td>");
	printf("%s", "<a href=#tmschapv2>EAP-PEAP MSCHAPv2</a><br>");
	printf("%s", "</td><td><div id=mschapv2>正在测试...</div></td></tr>\n");
	printf("%s", "<tr><td>");
	printf("%s", "<a href=#tpap>EAP-TTLS PAP</a><br>");
	printf("%s", "</td><td><div id=pap>正在测试...</div></td></tr>\n");
	printf("%s", "</table><p>\n");
	printf("%s", "<h2>结果说明：</h2>");
	printf("%s",
	       "如果本系统测试显示 \"<span style=\"color: green;\">OK，认证过程正常</span>\"，说明eduroam账号/密码均正确。<br>");
	printf("%s", "如果测试正常，但无法正确连接eduroam网络，建议联系当地eduroam网络服务提供商。<br>");
	printf("%s", "<h2>详细测试过程:</h2>");
	fflush(NULL);

	// step 1: EAP-PEAR MSCHAPv2 test
	printf("<a id=tmschapv2><h3>开始测试 EAP-PEAP MSCHAPv2 ...</h3></a>");

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
		"    ssid=\"eduroam\"\n"
		"    key_mgmt=WPA-EAP\n"
		"    eap=PEAP\n"
		"    identity=\"%s\"\n"
		"    anonymous_identity=\"%s\"\n"
		"    password=\"%s\"\n"
		"    phase2=\"autheap=MSCHAPV2\"\n" "}", login, login, pass);

	fclose(fp);

	// step 1.2: show config file
	printf("%s", "<h3>使用的配置文件</h3>");
	printf("<pre>\n");

	printf("network={\n"
		"    ssid=\"eduroam\"\n"
		"    key_mgmt=WPA-EAP\n"
		"    eap=PEAP\n"
		"    identity=\"%s\"\n"
		"    anonymous_identity=\"%s\"\n"
		"    password=\"%s\"\n"
		"    phase2=\"autheap=MSCHAPV2\"\n" "}", login, login, "********");
	printf("%s", "</pre>\n");
	fflush(NULL);

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
	if (ret != 0) {
		printf("%s",
		       "<script language=\"JavaScript\">document.getElementById(\"mschapv2\").innerHTML = \"<span style=\\\"color: red;\\\">FAILURE，认证失败</span>\"</script>");
		res = "<span style=\"color: red;\">FAILURE，认证失败</span>";
	} else {
		printf("%s",
		       "<script language=\"JavaScript\">document.getElementById(\"mschapv2\").innerHTML = \"<span style=\\\"color: green;\\\">OK，认证过程正常</span>\"</script>");
		res = "<span style=\"color: green;\">OK，认证过程正常</span>";
	}

	// step 1.4: show the result
	printf("<h3>测试结果: %s</h3>", res);
	printf("%s", "<pre>\n");
	fp = fopen(filename_out, "r");
	if (fp == NULL) {
		printf("open file %s error", filename_out);
		unlink(filename);
		exit(0);
	}
	while (fgets(buf, MAXLEN, fp))
		printf("%s", buf);
	fclose(fp);
	printf("%s", "</pre>\n");
	unlink(filename);
	unlink(filename_out);
	fflush(NULL);

	// step 2: EAP-TTLS PAP test
	printf("<a id=tpap><h3>开始测试 EAP-TTLS PAP ...</h3></a>");

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
		"    ssid=\"eduroam\"\n"
		"    key_mgmt=WPA-EAP\n"
		"    eap=TTLS\n"
		"    identity=\"%s\"\n"
		"    anonymous_identity=\"%s\"\n"
		"    password=\"%s\"\n" "    phase2=\"auth=PAP\"\n" "}", login, login, pass);
	fclose(fp);

	// step 2.2: show config file
	printf("%s", "<h3>使用的配置文件</h3>");

	printf("<pre>\n");
	printf("network={\n"
		"    ssid=\"eduroam\"\n"
		"    key_mgmt=WPA-EAP\n"
		"    eap=TTLS\n"
		"    identity=\"%s\"\n"
		"    anonymous_identity=\"%s\"\n"
		"    password=\"%s\"\n" "    phase2=\"auth=PAP\"\n" "}", login, login, "********");
	printf("%s", "</pre>\n");
	fflush(NULL);

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
	if (ret != 0) {
		printf("%s",
		       "<script language=\"JavaScript\">document.getElementById(\"pap\").innerHTML = \"<span style=\\\"color: red;\\\">FAILURE，认证失败</span>\"</script>");
		res = "<span style=\"color: red;\">FAILURE，认证失败</span>";
	} else {
		printf("%s",
		       "<script language=\"JavaScript\">document.getElementById(\"pap\").innerHTML = \"<span style=\\\"color: green;\\\">OK，认证过程正常</span>\"</script>");
		res = "<span style=\"color: green;\">OK，认证过程正常</span>";
	}

	// step 2.4: show the result
	printf("<h3>测试结果: %s</h3>", res);
	printf("%s", "<pre>\n");
	fp = fopen(filename_out, "r");
	if (fp == NULL) {
		printf("open file %s error", filename_out);
		unlink(filename);
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
