/* Radius SNMP support
 * Copyright (C) 2000 Jochen Friedrich <jochen@scram.de>
 *
 * You should have received a copy of the GNU General Public License
 * along with FreeRADIUS; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

static const char rcsid[] =
"$Id$";

#include "autoconf.h"

#ifdef WITH_SNMP

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <string.h>

#include <asn1.h>
#include <snmp.h>
#include <snmp_impl.h>

#include "smux.h"
#include "radiusd.h"

extern RADCLIENT *clients;
extern int need_reload;

#define RADACCOID 1,3,6,1,2,1,67,2,1,1,1
#define RADAUTHOID 1,3,6,1,2,1,67,1,1,1,1
#define RADIUSOID 1,3,6,1,4,1,3317,1,3,1

static oid radacc_oid [] = { RADACCOID };
static oid radauth_oid [] = { RADAUTHOID };
static oid radius_oid [] = { RADIUSOID };

#define COUNTER ASN_COUNTER
#define INTEGER ASN_INTEGER
#define GAUGE ASN_GAUGE
#define TIMETICKS ASN_TIMETICKS
#define IPADDRESS ASN_IPADDRESS
#define STRING ASN_OCTET_STR

#define RADIUSACCSERVIDENT               1
#define RADIUSACCSERVUPTIME              2
#define RADIUSACCSERVRESETTIME           3
#define RADIUSACCSERVCONFIGRESET         4
#define RADIUSACCSERVTOTALREQUESTS       5
#define RADIUSACCSERVTOTALINVALIDREQUESTS 6
#define RADIUSACCSERVTOTALDUPREQUESTS    7
#define RADIUSACCSERVTOTALRESPONSES      8
#define RADIUSACCSERVTOTALMALFORMEDREQUESTS 9
#define RADIUSACCSERVTOTALBADAUTHENTICATORS 10
#define RADIUSACCSERVTOTALPACKETSDROPPED 11
#define RADIUSACCSERVTOTALNORECORDS      12
#define RADIUSACCSERVTOTALUNKNOWNTYPES   13
/* */
#define RADIUSACCCLIENTADDRESS           2
#define RADIUSACCCLIENTID                3
#define RADIUSACCSERVPACKETSDROPPED      4
#define RADIUSACCSERVREQUESTS            5
#define RADIUSACCSERVDUPREQUESTS         6
#define RADIUSACCSERVRESPONSES           7
#define RADIUSACCSERVBADAUTHENTICATORS   8
#define RADIUSACCSERVMALFORMEDREQUESTS   9
#define RADIUSACCSERVNORECORDS           10
#define RADIUSACCSERVUNKNOWNTYPES        11
/* */
#define RADIUSAUTHSERVIDENT              1
#define RADIUSAUTHSERVUPTIME             2
#define RADIUSAUTHSERVRESETTIME          3
#define RADIUSAUTHSERVCONFIGRESET        4
#define RADIUSAUTHSERVTOTALACCESSREQUESTS 5
#define RADIUSAUTHSERVTOTALINVALIDREQUESTS 6
#define RADIUSAUTHSERVTOTALDUPACCESSREQUESTS 7
#define RADIUSAUTHSERVTOTALACCESSACCEPTS 8
#define RADIUSAUTHSERVTOTALACCESSREJECTS 9
#define RADIUSAUTHSERVTOTALACCESSCHALLENGES 10
#define RADIUSAUTHSERVTOTALMALFORMEDACCESSREQUESTS 11
#define RADIUSAUTHSERVTOTALBADAUTHENTICATORS 12
#define RADIUSAUTHSERVTOTALPACKETSDROPPED 13
#define RADIUSAUTHSERVTOTALUNKNOWNTYPES  14
/* */
#define RADIUSAUTHCLIENTADDRESS          2
#define RADIUSAUTHCLIENTID               3
#define RADIUSAUTHSERVACCESSREQUESTS     4
#define RADIUSAUTHSERVDUPACCESSREQUESTS  5
#define RADIUSAUTHSERVACCESSACCEPTS      6
#define RADIUSAUTHSERVACCESSREJECTS      7
#define RADIUSAUTHSERVACCESSCHALLENGES   8
#define RADIUSAUTHSERVMALFORMEDACCESSREQUESTS 9
#define RADIUSAUTHSERVBADAUTHENTICATORS  10
#define RADIUSAUTHSERVPACKETSDROPPED     11
#define RADIUSAUTHSERVUNKNOWNTYPES       12

/* Hook functions. */
static u_char *radAccServ ();
static u_char *radAccEntry ();
static u_char *radAuthServ ();
static u_char *radAuthEntry ();

static struct variable radiusacc_variables[] = 
{
  {RADIUSACCSERVIDENT, STRING, RONLY, radAccServ, 1, {1}},
  {RADIUSACCSERVUPTIME, TIMETICKS, RONLY, radAccServ, 1, {2}},
  {RADIUSACCSERVRESETTIME, TIMETICKS, RONLY, radAccServ, 1, {3}},
  {RADIUSACCSERVCONFIGRESET, INTEGER, RWRITE, radAccServ, 1, {4}},
  {RADIUSACCSERVTOTALREQUESTS, COUNTER, RONLY, radAccServ, 1, {5}},
  {RADIUSACCSERVTOTALINVALIDREQUESTS, COUNTER, RONLY, radAccServ, 1, {6}},
  {RADIUSACCSERVTOTALDUPREQUESTS, COUNTER, RONLY, radAccServ, 1, {7}},
  {RADIUSACCSERVTOTALRESPONSES, COUNTER, RONLY, radAccServ, 1, {8}},
  {RADIUSACCSERVTOTALMALFORMEDREQUESTS, COUNTER, RONLY, radAccServ, 1, {9}},
  {RADIUSACCSERVTOTALBADAUTHENTICATORS, COUNTER, RONLY, radAccServ, 1, {10}},
  {RADIUSACCSERVTOTALPACKETSDROPPED, COUNTER, RONLY, radAccServ, 1, {11}},
  {RADIUSACCSERVTOTALNORECORDS, COUNTER, RONLY, radAccServ, 1, {12}},
  {RADIUSACCSERVTOTALUNKNOWNTYPES, COUNTER, RONLY, radAccServ, 1, {13}},
  {RADIUSACCCLIENTADDRESS, IPADDRESS, RONLY, radAccEntry, 3, {14,1,2}},
  {RADIUSACCCLIENTID, STRING, RONLY, radAccEntry, 3, {14,1,3}},
  {RADIUSACCSERVPACKETSDROPPED, COUNTER, RONLY, radAccEntry, 3, {14,1,4}},
  {RADIUSACCSERVREQUESTS, COUNTER, RONLY, radAccEntry, 3, {14,1,5}},
  {RADIUSACCSERVDUPREQUESTS, COUNTER, RONLY, radAccEntry, 3, {14,1,6}},
  {RADIUSACCSERVRESPONSES, COUNTER, RONLY, radAccEntry, 3, {14,1,7}},
  {RADIUSACCSERVBADAUTHENTICATORS, COUNTER, RONLY, radAccEntry, 3, {14,1,8}},
  {RADIUSACCSERVMALFORMEDREQUESTS, COUNTER, RONLY, radAccEntry, 3, {14,1,9}},
  {RADIUSACCSERVNORECORDS, COUNTER, RONLY, radAccEntry, 3, {14,1,10}},
  {RADIUSACCSERVUNKNOWNTYPES, COUNTER, RONLY, radAccEntry, 3, {14,1,11}},
};

static struct variable radiusauth_variables[] =
{
  {RADIUSAUTHSERVIDENT, STRING, RONLY, radAuthServ, 1, {1}},
  {RADIUSAUTHSERVUPTIME, TIMETICKS, RONLY, radAuthServ, 1, {2}},
  {RADIUSAUTHSERVRESETTIME, TIMETICKS, RONLY, radAuthServ, 1, {3}},
  {RADIUSAUTHSERVCONFIGRESET, INTEGER, RWRITE, radAuthServ, 1, {4}},
  {RADIUSAUTHSERVTOTALACCESSREQUESTS, COUNTER, RONLY, radAuthServ, 1, {5}},
  {RADIUSAUTHSERVTOTALINVALIDREQUESTS, COUNTER, RONLY, radAuthServ, 1, {6}},
  {RADIUSAUTHSERVTOTALDUPACCESSREQUESTS, COUNTER, RONLY, radAuthServ, 1, {7}},
  {RADIUSAUTHSERVTOTALACCESSACCEPTS, COUNTER, RONLY, radAuthServ, 1, {8}},
  {RADIUSAUTHSERVTOTALACCESSREJECTS, COUNTER, RONLY, radAuthServ, 1, {9}},
  {RADIUSAUTHSERVTOTALACCESSCHALLENGES, COUNTER, RONLY, radAuthServ, 1, {10}},
  {RADIUSAUTHSERVTOTALMALFORMEDACCESSREQUESTS, COUNTER, RONLY, radAuthServ, 1,
    {11}},
  {RADIUSAUTHSERVTOTALBADAUTHENTICATORS, COUNTER, RONLY, radAuthServ, 1, {12}},
  {RADIUSAUTHSERVTOTALPACKETSDROPPED, COUNTER, RONLY, radAuthServ, 1, {13}},
  {RADIUSAUTHSERVTOTALUNKNOWNTYPES, COUNTER, RONLY, radAuthServ, 1, {14}},
  {RADIUSAUTHCLIENTADDRESS, IPADDRESS, RONLY, radAuthEntry, 3, {15,1,2}},
  {RADIUSAUTHCLIENTID, STRING, RONLY, radAuthEntry, 3, {15,1,3}},
  {RADIUSAUTHSERVACCESSREQUESTS, COUNTER, RONLY, radAuthEntry, 3, {15,1,4}},
  {RADIUSAUTHSERVDUPACCESSREQUESTS, COUNTER, RONLY, radAuthEntry, 3, {15,1,5}},
  {RADIUSAUTHSERVACCESSACCEPTS, COUNTER, RONLY, radAuthEntry, 3, {15,1,6}},
  {RADIUSAUTHSERVACCESSREJECTS, COUNTER, RONLY, radAuthEntry, 3, {15,1,7}},
  {RADIUSAUTHSERVACCESSCHALLENGES, COUNTER, RONLY, radAuthEntry, 3, {15,1,8}},
  {RADIUSAUTHSERVMALFORMEDACCESSREQUESTS, COUNTER, RONLY, radAuthEntry, 3, 
    {15,1,9}},
  {RADIUSAUTHSERVBADAUTHENTICATORS, COUNTER, RONLY, radAuthEntry, 3, {15,1,10}},
  {RADIUSAUTHSERVPACKETSDROPPED, COUNTER, RONLY, radAuthEntry, 3, {15,1,11}},
  {RADIUSAUTHSERVUNKNOWNTYPES, COUNTER, RONLY, radAuthEntry, 3, {15,1,12}},
};


static RADCLIENT *
get_client(struct variable *v, oid objid[], size_t *objid_len, int exact)
{
  RADCLIENT *c;
  int i, len;

  len = *objid_len - v->namelen;

  if (!clients)
    return NULL;

  if (exact)
    {
      /* Check the length. */
      if (len != 1)
	return NULL;
      if (objid[v->namelen] == 0)
	return NULL;

      i = objid[v->namelen]-1;
      c = clients;
      while (i && c)
	{
	  c = c->next;
	  i--;
	}
      if (c)
	return c;
      return NULL;
    }
  i = objid[v->namelen]-1;
  *objid_len = v->namelen + 1;
  if (!len || (objid[v->namelen] == 0))
    {
      objid[v->namelen]=1;
      return clients;
    }
  c = clients->next;
  while (i && c)
    {
      c = c->next;
      i--;
    }
  if (c)
    {
      objid[v->namelen]++;
      return c;
    }
  else
    return NULL;
}

static int
radServReset (int action,
    u_char   *var_val,
    u_char   var_val_type,
    size_t   var_val_len,
    u_char   *statP,
    oid      *name,
    size_t   name_len)
{
    long i;
    int big = SNMP_MAX_LEN;

    switch (action)
      {
	case RESERVE1:
	  if (var_val_type != INTEGER)
	    return SNMP_ERR_WRONGTYPE;
	  if (var_val_len != sizeof (long))
	    return SNMP_ERR_WRONGLENGTH;
	  if (! asn_parse_int(var_val, &big, &var_val_type, &i, sizeof(long)))
	    return SNMP_ERR_WRONGENCODING;
	  if (i != 2)
	    return SNMP_ERR_WRONGVALUE;
	  break;
	case COMMIT:
	  need_reload = TRUE;
	  break;
	case FREE:
	  break;
	default:
	  return SNMP_ERR_GENERR;
      }
    return SNMP_ERR_NOERROR;
}

static unsigned char *
radAccServ(struct variable *vp,
    oid     *name,
    size_t  *length,
    int     exact,
    size_t  *var_len,
    WriteMethod **write_method)
{
    static int result;
    /* check whether the instance identifier is valid */
    if (smux_header_generic(vp, name, length, exact, var_len,
                       write_method) == MATCH_FAILED) {
        return NULL;
    }

    /* return the current value of the variable */
    switch (vp->magic) {

    case RADIUSACCSERVIDENT:
	    *var_len = rad_snmp.acct.ident;
        return (unsigned char *) rad_snmp.acct.ident;

    case RADIUSACCSERVUPTIME:
	rad_snmp.acct.uptime = (time(NULL) - rad_snmp.acct.start_time) * 100;
        return (unsigned char *) &rad_snmp.acct.uptime;

    case RADIUSACCSERVRESETTIME:
	rad_snmp.acct.reset_time = (time(NULL) - rad_snmp.acct.last_reset_time) * 100;
        return (unsigned char *) &rad_snmp.acct.reset_time;

    case RADIUSACCSERVCONFIGRESET:
	*write_method = radServReset;
	result = 4;
        return (unsigned char *) &result;

    case RADIUSACCSERVTOTALREQUESTS:
        return (unsigned char *) &rad_snmp.acct.total_requests;

    case RADIUSACCSERVTOTALINVALIDREQUESTS:
        return (unsigned char *) &rad_snmp.acct.total_invalid_requests;

    case RADIUSACCSERVTOTALDUPREQUESTS:
        return (unsigned char *) &rad_snmp.acct.total_dup_requests;

    case RADIUSACCSERVTOTALRESPONSES:
        return (unsigned char *) &rad_snmp.acct.total_responses;

    case RADIUSACCSERVTOTALMALFORMEDREQUESTS:
        return (unsigned char *) &rad_snmp.acct.total_malformed_requests;

    case RADIUSACCSERVTOTALBADAUTHENTICATORS:
        return (unsigned char *) &rad_snmp.acct.total_bad_authenticators;

    case RADIUSACCSERVTOTALPACKETSDROPPED:
        return (unsigned char *) &rad_snmp.acct.total_packets_dropped;

    case RADIUSACCSERVTOTALNORECORDS:
        return (unsigned char *) &rad_snmp.acct.total_no_records;

    case RADIUSACCSERVTOTALUNKNOWNTYPES:
        return (unsigned char *) &rad_snmp.acct.total_unknown_types;

    }

    return NULL;
}

static unsigned char *
radAccEntry(struct variable *vp,
    oid     *name,
    size_t  *length,
    int     exact,
    size_t  *var_len,
    WriteMethod **write_method)
{
    RADCLIENT *c;

    *write_method = NULL; /* table is read only */
    c = get_client(vp, name, length, exact);
    if (!c)
      return NULL;

    /* return the current value of the variable */

    switch (vp->magic) {

    case RADIUSACCCLIENTADDRESS:
        *var_len = sizeof(c->ipaddr);
        return (unsigned char *)&(c->ipaddr);

    case RADIUSACCCLIENTID:
	if (strlen(c->shortname)) {
		*var_len = strlen(c->shortname);
        	return c->shortname;
	}
        *var_len = strlen(c->longname);
        return c->longname;

    case RADIUSACCSERVPACKETSDROPPED:
        return (unsigned char *) NULL;

    case RADIUSACCSERVREQUESTS:
        return (unsigned char *) NULL;

    case RADIUSACCSERVDUPREQUESTS:
        return (unsigned char *) NULL;

    case RADIUSACCSERVRESPONSES:
        return (unsigned char *) NULL;

    case RADIUSACCSERVBADAUTHENTICATORS:
        return (unsigned char *) NULL;

    case RADIUSACCSERVMALFORMEDREQUESTS:
        return (unsigned char *) NULL;

    case RADIUSACCSERVNORECORDS:
        return (unsigned char *) NULL;

    case RADIUSACCSERVUNKNOWNTYPES:
        return (unsigned char *) NULL;

    }
    return NULL;
}

static unsigned char *
radAuthServ(struct variable *vp,
    oid     *name,
    size_t  *length,
    int     exact,
    size_t  *var_len,
    WriteMethod **write_method)
{
    static int result;
    /* check whether the instance identifier is valid */

    if (smux_header_generic(vp, name, length, exact, var_len,
                       write_method) == MATCH_FAILED) {
        return NULL;
    }

    /* return the current value of the variable */

    switch (vp->magic) {

    case RADIUSAUTHSERVIDENT:
	*var_len = rad_snmp.auth.ident;
        return (unsigned char *) rad_snmp.auth.ident;

    case RADIUSAUTHSERVUPTIME:
	rad_snmp.auth.uptime = (time(NULL) - rad_snmp.auth.start_time) * 100;
        return (unsigned char *) &rad_snmp.auth.uptime;

    case RADIUSAUTHSERVRESETTIME:
	rad_snmp.auth.reset_time = (time(NULL) - rad_snmp.auth.last_reset_time) * 100;
        return (unsigned char *) &rad_snmp.auth.reset_time;

    case RADIUSAUTHSERVCONFIGRESET:
	*write_method = radServReset;
	result = 4;
        return (unsigned char *) &result;

    case RADIUSAUTHSERVTOTALACCESSREQUESTS:
        return (unsigned char *) &rad_snmp.auth.total_requests;

    case RADIUSAUTHSERVTOTALINVALIDREQUESTS:
        return (unsigned char *) &rad_snmp.auth.total_invalid_requests;

    case RADIUSAUTHSERVTOTALDUPACCESSREQUESTS:
        return (unsigned char *) &rad_snmp.auth.total_dup_requests;

    case RADIUSAUTHSERVTOTALACCESSACCEPTS:
        return (unsigned char *) &rad_snmp.auth.total_access_accepts;

    case RADIUSAUTHSERVTOTALACCESSREJECTS:
        return (unsigned char *) &rad_snmp.auth.total_access_rejects;

    case RADIUSAUTHSERVTOTALACCESSCHALLENGES:
        return (unsigned char *) &rad_snmp.auth.total_access_challenges;

    case RADIUSAUTHSERVTOTALMALFORMEDACCESSREQUESTS:
        return (unsigned char *) &rad_snmp.auth.total_malformed_requests;

    case RADIUSAUTHSERVTOTALBADAUTHENTICATORS:
        return (unsigned char *) &rad_snmp.auth.total_bad_authenticators;

    case RADIUSAUTHSERVTOTALPACKETSDROPPED:
        return (unsigned char *) &rad_snmp.auth.total_packets_dropped;

    case RADIUSAUTHSERVTOTALUNKNOWNTYPES:
        return (unsigned char *) &rad_snmp.auth.total_unknown_types;

    }

    return NULL;
}

static unsigned char *
radAuthEntry(struct variable *vp,
    oid     *name,
    size_t  *length,
    int     exact,
    size_t  *var_len,
    WriteMethod **write_method)
{
    RADCLIENT *c;

    *write_method = NULL; /* table is read only */
    c = get_client(vp, name, length, exact);
    if (!c)
      return NULL;

    /* return the current value of the variable */

    switch (vp->magic) {

    case RADIUSAUTHCLIENTADDRESS:
        *var_len = sizeof(c->ipaddr);
        return (unsigned char *)&(c->ipaddr);

    case RADIUSAUTHCLIENTID:
	if (strlen(c->shortname)) {
		*var_len = strlen(c->shortname);
        	return c->shortname;
	}
        *var_len = strlen(c->longname);
        return c->longname;

    case RADIUSAUTHSERVACCESSREQUESTS:
        return (unsigned char *) NULL;

    case RADIUSAUTHSERVDUPACCESSREQUESTS:
        return (unsigned char *) NULL;

    case RADIUSAUTHSERVACCESSACCEPTS:
        return (unsigned char *) NULL;

    case RADIUSAUTHSERVACCESSREJECTS:
        return (unsigned char *) NULL;

    case RADIUSAUTHSERVACCESSCHALLENGES:
        return (unsigned char *) NULL;

    case RADIUSAUTHSERVMALFORMEDACCESSREQUESTS:
        return (unsigned char *) NULL;

    case RADIUSAUTHSERVBADAUTHENTICATORS:
        return (unsigned char *) NULL;

    case RADIUSAUTHSERVPACKETSDROPPED:
        return (unsigned char *) NULL;

    case RADIUSAUTHSERVUNKNOWNTYPES:
        return (unsigned char *) NULL;

      }
    return NULL;
}


/* Register RADIUS MIBs. */
void
radius_snmp_init (void)
{
  memset(&rad_snmp, 0, sizeof(rad_snmp));

  rad_snmp.auth.ident = "FreeRADIUS v0.1.0";
  rad_snmp.acct.ident = "FreeRADIUS v0.1.0";

  /*
   *  We really should get better clock resolution..
   */
  rad_snmp.auth.start_time = time(NULL);
  rad_snmp.auth.last_reset_time = rad_snmp.auth.start_time;

  rad_snmp.acct.start_time = rad_snmp.auth.start_time;
  rad_snmp.acct.last_reset_time = rad_snmp.auth.start_time;

  smux_init (radius_oid, sizeof (radius_oid) / sizeof (oid));
  REGISTER_MIB("mibII/radius-acc-server", radiusacc_variables, variable, radacc_oid);
  REGISTER_MIB("mibII/radius-auth-server", radiusauth_variables, variable, radauth_oid);
  smux_start ();
}

#endif /* WITH_SNMP */
