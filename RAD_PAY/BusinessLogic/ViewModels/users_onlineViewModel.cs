using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class users_onlineViewModel
    {
        public long uid { get; set; }
        public string token { get; set; }
        public DateTime login_time { get; set; }
        public DateTime last_online { get; set; }
        public long? dev_id { get; set; }
    }
}