using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class user_devicesViewModel
    {
        public long dev_id { get; set; }
        public long? uid { get; set; }
        public string dev_token { get; set; }
        public string password { get; set; }
        public DateTime? add_ts { get; set; }
        public string notify_token { get; set; }
        public string os { get; set; }
        public DateTime? login_ts { get; set; }
        public string dev_imei { get; set; }
    }
}