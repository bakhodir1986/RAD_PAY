using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class merchant_access_infoViewModel
    {
        public int merchant_id { get; set; }
        public string login { get; set; }
        public string password { get; set; }
        public string api_json { get; set; }
        public string url { get; set; }
    }
}